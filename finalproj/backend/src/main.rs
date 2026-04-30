use std::{env, net::SocketAddr, path::PathBuf, sync::Arc};

use axum::{
    extract::{Form, Query, State},
    http::StatusCode,
    response::{Html, IntoResponse, Redirect, Response},
    routing::{get, post},
    Json, Router,
};
use base64::{prelude::BASE64_STANDARD, Engine};
use chrono::{DateTime, Local, TimeZone};
use dotenvy::dotenv;
use mqtt5::{ConnectOptions, Message, MqttClient};
use serde::{de::Error as DeError, Deserialize, Deserializer, Serialize};
use serde_json::from_str;
use sqlx::{sqlite::SqlitePoolOptions, Pool, Sqlite};
use tera::{Context, Tera};
use tower_http::services::ServeDir;

use crate::{
    db::{
        fetch_latest_for_location, fetch_latest_location_snapshots, fetch_recent_location_history,
        fetch_records, fetch_summary, init_db, insert_db, list_locations, update_location_capacity,
        LocationHistoryRow, LocationLiveRow, LocationOption, LocationSnapshotRow, RecordRow,
        SummaryRow,
    },
    parse::parse_app_message,
};

mod db;
pub mod parse;

const TOPIC: &str = "v3/+/devices/+/up";

#[derive(Clone)]
struct AppState {
    pool: Pool<Sqlite>,
    templates: Arc<Tera>,
}

#[derive(Debug, Deserialize)]
struct Data {
    uplink_message: UplinkMessage,
    end_device_ids: EndDeviceIds,
    received_at: DateTime<Local>,
}

#[derive(Debug, Deserialize)]
struct UplinkMessage {
    frm_payload: String,
}

#[derive(Debug, Deserialize)]
struct EndDeviceIds {
    device_id: String,
}

#[derive(Debug, Deserialize)]
struct DashboardFilters {
    #[serde(default, deserialize_with = "empty_string_as_none_i64")]
    location_id: Option<i64>,
    device_name: Option<String>,
    #[serde(default, deserialize_with = "empty_string_as_none_i64")]
    limit: Option<i64>,
}

#[derive(Debug, Deserialize)]
struct LivePageFilters {
    #[serde(default, deserialize_with = "empty_string_as_none_i64")]
    location_id: Option<i64>,
}

#[derive(Debug, Deserialize)]
struct LiveApiFilters {
    #[serde(default, deserialize_with = "empty_string_as_none_i64")]
    location_id: Option<i64>,
    #[serde(default, deserialize_with = "empty_string_as_none_i64")]
    limit: Option<i64>,
}

#[derive(Debug, Deserialize)]
struct CapacityUpdateForm {
    location_id: i64,
    #[serde(default, deserialize_with = "empty_string_as_none_i64")]
    capacity: Option<i64>,
    return_to: Option<String>,
}

#[derive(Debug, Deserialize)]
struct AppMessage {
    total_devices: u16,
    personal_devices: u16,
    mobile_devices: u16,
    laptops: u16,
    wearables: u16,
    unknowns: u16,
    apple_devices: u16,
    google_devices: u16,
    microsoft_devices: u16,
    samsung_devices: u16,
    location_id: u8,
    xor_checksum: u8,
}

#[derive(Debug, Serialize)]
struct DashboardPage {
    filters: DashboardFilterView,
    summary: SummaryView,
    records: Vec<RecordView>,
    locations: Vec<LocationView>,
    location_snapshots: Vec<LocationSnapshotView>,
}

#[derive(Debug, Serialize)]
struct LivePage {
    selected_location_id: i64,
    selected_location_name: String,
    selected_location_capacity: Option<i64>,
    selected_location_capacity_display: String,
    selected_location_capacity_input: String,
    selected_location_capacity_json: String,
    latest_sample: Option<LiveSampleView>,
    history: Vec<HistoryPointView>,
    history_json: String,
    locations: Vec<LocationView>,
}

#[derive(Debug, Serialize)]
struct DashboardFilterView {
    selected_device_name: String,
    limit: i64,
}

#[derive(Debug, Serialize)]
struct LocationView {
    location_id: i64,
    location_name: String,
    capacity: Option<i64>,
    capacity_display: String,
    selected: bool,
}

#[derive(Debug, Serialize)]
struct SummaryView {
    sample_count: i64,
    avg_total: String,
    max_total: i64,
    latest_seen: String,
}

#[derive(Debug, Serialize)]
struct RecordView {
    id: i64,
    location_name: String,
    device_name: String,
    total: i64,
    personal: i64,
    mobiles: i64,
    laptops: i64,
    wearables: i64,
    unknowns: i64,
    apples: i64,
    googles: i64,
    microsofts: i64,
    samsungs: i64,
    captured_at: String,
}

#[derive(Debug, Serialize)]
struct LocationSnapshotView {
    location_id: i64,
    location_name: String,
    capacity: Option<i64>,
    capacity_display: String,
    estimated_occupants: i64,
    capacity_status: CapacityStatusView,
    total: i64,
    personal: i64,
    mobiles: i64,
    laptops: i64,
    wearables: i64,
    captured_at: String,
}

#[derive(Debug, Serialize)]
struct LiveSampleView {
    location_name: String,
    capacity: Option<i64>,
    capacity_display: String,
    estimated_occupants: i64,
    capacity_status: CapacityStatusView,
    device_name: String,
    total: i64,
    personal: i64,
    mobiles: i64,
    laptops: i64,
    wearables: i64,
    unknowns: i64,
    apples: i64,
    googles: i64,
    microsofts: i64,
    samsungs: i64,
    captured_at: String,
    captured_at_unix: i64,
}

#[derive(Debug, Serialize)]
struct HistoryPointView {
    total: i64,
    personal: i64,
    mobiles: i64,
    laptops: i64,
    wearables: i64,
    captured_at: String,
    captured_at_unix: i64,
}

#[derive(Debug, Serialize)]
struct LiveApiResponse {
    location_id: i64,
    location_name: String,
    capacity: Option<i64>,
    latest_sample: Option<LiveSampleView>,
    history: Vec<HistoryPointView>,
    polled_at: String,
}

#[derive(Debug, Serialize, Clone)]
struct CapacityStatusView {
    state: &'static str,
    label: String,
    detail: String,
}

impl AppMessage {
    fn get_checksum(&self) -> u8 {
        let mut checksum = 0x00;
        checksum ^= self.total_devices as u8 & 0xFF;
        checksum ^= (self.total_devices >> 8) as u8 & 0xFF;
        checksum ^= self.personal_devices as u8 & 0xFF;
        checksum ^= (self.personal_devices >> 8) as u8 & 0xFF;
        checksum ^= self.mobile_devices as u8 & 0xFF;
        checksum ^= (self.mobile_devices >> 8) as u8 & 0xFF;
        checksum ^= self.laptops as u8 & 0xFF;
        checksum ^= (self.laptops >> 8) as u8 & 0xFF;
        checksum ^= self.wearables as u8 & 0xFF;
        checksum ^= (self.wearables >> 8) as u8 & 0xFF;
        checksum ^= self.unknowns as u8 & 0xFF;
        checksum ^= (self.unknowns >> 8) as u8 & 0xFF;
        checksum ^= self.apple_devices as u8 & 0xFF;
        checksum ^= (self.apple_devices >> 8) as u8 & 0xFF;
        checksum ^= self.google_devices as u8 & 0xFF;
        checksum ^= (self.google_devices >> 8) as u8 & 0xFF;
        checksum ^= self.microsoft_devices as u8 & 0xFF;
        checksum ^= (self.microsoft_devices >> 8) as u8 & 0xFF;
        checksum ^= self.samsung_devices as u8 & 0xFF;
        checksum ^= (self.samsung_devices >> 8) as u8 & 0xFF;
        checksum ^= self.location_id;
        checksum
    }
}

fn empty_string_as_none_i64<'de, D>(deserializer: D) -> Result<Option<i64>, D::Error>
where
    D: Deserializer<'de>,
{
    let value = Option::<String>::deserialize(deserializer)?;
    match value.as_deref().map(str::trim) {
        None | Some("") => Ok(None),
        Some(value) => value.parse::<i64>().map(Some).map_err(D::Error::custom),
    }
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let _ = dotenv();

    let pool = SqlitePoolOptions::new()
        .max_connections(5)
        .connect(&database_url())
        .await?;
    init_db(&pool).await;

    let templates = Arc::new(load_templates()?);
    let state = AppState {
        pool: pool.clone(),
        templates,
    };

    let app = Router::new()
        .route("/", get(index))
        .route("/live", get(live_page))
        .route("/api/live-data", get(live_data))
        .route("/locations/capacity", post(save_location_capacity))
        .nest_service("/static", ServeDir::new(frontend_static_dir()))
        .with_state(state);

    let http_addr: SocketAddr = env::var("APP_ADDRESS")
        .unwrap_or_else(|_| "127.0.0.1:3000".to_string())
        .parse()?;
    let listener = tokio::net::TcpListener::bind(http_addr).await?;

    println!("Serving dashboard at http://{http_addr}");

    let mqtt_task = maybe_start_mqtt(pool.clone());
    let http_task = async move {
        axum::serve(listener, app)
            .await
            .map_err(Box::<dyn std::error::Error>::from)
    };

    tokio::select! {
        result = http_task => result?,
        result = mqtt_task => result?,
    }

    Ok(())
}

async fn index(
    State(state): State<AppState>,
    Query(filters): Query<DashboardFilters>,
) -> Result<Html<String>, AppError> {
    let limit = filters.limit.unwrap_or(50).clamp(1, 250);
    let device_name = filters
        .device_name
        .as_deref()
        .map(str::trim)
        .filter(|value| !value.is_empty());

    let records = fetch_records(&state.pool, filters.location_id, device_name, limit).await?;
    let summary = fetch_summary(&state.pool, filters.location_id, device_name).await?;
    let locations = list_locations(&state.pool).await?;
    let location_snapshots = fetch_latest_location_snapshots(&state.pool).await?;

    let page = DashboardPage {
        filters: DashboardFilterView {
            selected_device_name: filters.device_name.unwrap_or_default(),
            limit,
        },
        summary: summary_view(summary),
        records: records.into_iter().map(record_view).collect(),
        locations: locations
            .into_iter()
            .map(|location| location_view(location, filters.location_id))
            .collect(),
        location_snapshots: location_snapshots
            .into_iter()
            .map(location_snapshot_view)
            .collect(),
    };

    let mut context = Context::new();
    context.insert("page", &page);

    let html = state.templates.render("index.html", &context)?;
    Ok(Html(html))
}

async fn live_page(
    State(state): State<AppState>,
    Query(filters): Query<LivePageFilters>,
) -> Result<Html<String>, AppError> {
    let locations = list_locations(&state.pool).await?;
    let selected_location = select_location(&locations, filters.location_id)?;
    let selected_location_id = selected_location.location_id;
    let selected_location_name = selected_location.location_name.clone();
    let latest = fetch_latest_for_location(&state.pool, selected_location_id).await?;
    let history = fetch_recent_location_history(&state.pool, selected_location_id, 36).await?;
    let history_views: Vec<_> = history.into_iter().map(history_point_view).collect();

    let page = LivePage {
        selected_location_id,
        selected_location_name,
        selected_location_capacity: selected_location.capacity,
        selected_location_capacity_display: format_capacity(selected_location.capacity),
        selected_location_capacity_input: selected_location
            .capacity
            .map(|value| value.to_string())
            .unwrap_or_default(),
        selected_location_capacity_json: serde_json::to_string(&selected_location.capacity)
            .map_err(|error| AppError(error.to_string()))?,
        latest_sample: latest.map(live_sample_view),
        history_json: serde_json::to_string(&history_views)
            .map_err(|error| AppError(error.to_string()))?,
        history: history_views,
        locations: locations
            .into_iter()
            .map(|location| location_view(location, Some(selected_location_id)))
            .collect(),
    };

    let mut context = Context::new();
    context.insert("page", &page);

    let html = state.templates.render("live.html", &context)?;
    Ok(Html(html))
}

async fn live_data(
    State(state): State<AppState>,
    Query(filters): Query<LiveApiFilters>,
) -> Result<Json<LiveApiResponse>, AppError> {
    let locations = list_locations(&state.pool).await?;
    let selected_location = select_location(&locations, filters.location_id)?;
    let selected_location_id = selected_location.location_id;
    let selected_location_name = selected_location.location_name.clone();
    let limit = filters.limit.unwrap_or(36).clamp(8, 120);
    let latest = fetch_latest_for_location(&state.pool, selected_location_id).await?;
    let history = fetch_recent_location_history(&state.pool, selected_location_id, limit).await?;

    Ok(Json(LiveApiResponse {
        location_id: selected_location_id,
        location_name: selected_location_name,
        capacity: selected_location.capacity,
        latest_sample: latest.map(live_sample_view),
        history: history.into_iter().map(history_point_view).collect(),
        polled_at: format_timestamp(Some(Local::now().timestamp())),
    }))
}

async fn save_location_capacity(
    State(state): State<AppState>,
    Form(form): Form<CapacityUpdateForm>,
) -> Result<Redirect, AppError> {
    if let Some(capacity) = form.capacity {
        if capacity < 0 {
            return Err(AppError("Capacity must be zero or greater".to_string()));
        }
    }

    update_location_capacity(&state.pool, form.location_id, form.capacity).await?;

    let target = form
        .return_to
        .filter(|value| value.starts_with('/'))
        .unwrap_or_else(|| format!("/live?location_id={}", form.location_id));

    Ok(Redirect::to(&target))
}

async fn maybe_start_mqtt(pool: Pool<Sqlite>) -> Result<(), Box<dyn std::error::Error>> {
    let address = match env::var("TTN_BROKER") {
        Ok(value) => value,
        Err(_) => {
            println!("TTN_BROKER not set; skipping MQTT listener");
            return Ok(std::future::pending::<()>().await);
        }
    };
    let username = env::var("TTN_USERNAME")?;
    let password = env::var("TTN_PASSWORD")?;

    println!("Connecting to mqtt server...");
    let client = MqttClient::with_options(
        ConnectOptions::new("final-proj-listener")
            .with_protocol_version(mqtt5::ProtocolVersion::V311)
            .with_credentials(username, password),
    );
    client.connect(&format!("mqtts://{address}:8883")).await?;

    println!("Subscribing to mqtt topic...");
    client
        .subscribe(TOPIC, move |message| {
            let pool = pool.clone();
            tokio::spawn(async move {
                receive_message(message, pool).await;
            });
        })
        .await?;

    println!("MQTT listener is active");
    std::future::pending::<()>().await;
    Ok(())
}

async fn receive_message(message: Message, pool: Pool<Sqlite>) {
    let payload = String::from_utf8_lossy(&message.payload);
    let parsed: Data = match from_str(&payload) {
        Ok(value) => value,
        Err(error) => {
            eprintln!("Skipping malformed TTN payload: {error}");
            return;
        }
    };

    let data = match BASE64_STANDARD.decode(parsed.uplink_message.frm_payload) {
        Ok(value) => value,
        Err(error) => {
            eprintln!("Skipping invalid base64 payload: {error}");
            return;
        }
    };

    let device_id = parsed.end_device_ids.device_id;
    let (_, app_message) = match parse_app_message(&data) {
        Ok(value) => value,
        Err(error) => {
            eprintln!("Skipping unparseable message from {device_id}: {error}");
            return;
        }
    };

    if app_message.xor_checksum != app_message.get_checksum() {
        eprintln!("Skipping message from {device_id}: checksum mismatch");
        return;
    }

    insert_db(
        app_message,
        device_id,
        parsed.received_at.timestamp(),
        &pool,
    )
    .await;
}

fn database_url() -> String {
    env::var("DATABASE_URL").unwrap_or_else(|_| {
        let db_path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("database.db");
        format!("sqlite://{}", db_path.display())
    })
}

fn frontend_root() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .expect("backend should live in repo root")
        .join("frontend")
}

fn frontend_static_dir() -> PathBuf {
    frontend_root().join("static")
}

fn load_templates() -> Result<Tera, tera::Error> {
    let pattern = format!("{}/templates/**/*", frontend_root().display());
    let mut tera = Tera::new(&pattern)?;
    tera.autoescape_on(vec![]);
    Ok(tera)
}

fn summary_view(summary: SummaryRow) -> SummaryView {
    SummaryView {
        sample_count: summary.sample_count,
        avg_total: format!("{:.1}", summary.avg_total),
        max_total: summary.max_total,
        latest_seen: format_timestamp(summary.latest_time),
    }
}

fn record_view(record: RecordRow) -> RecordView {
    RecordView {
        id: record.id,
        location_name: record.location_name,
        device_name: record.device_name,
        total: record.total,
        personal: record.personal,
        mobiles: record.mobiles,
        laptops: record.laptops,
        wearables: record.wearables,
        unknowns: record.unknowns,
        apples: record.apples,
        googles: record.googles,
        microsofts: record.microsofts,
        samsungs: record.samsungs,
        captured_at: format_timestamp(Some(record.time)),
    }
}

fn location_snapshot_view(snapshot: LocationSnapshotRow) -> LocationSnapshotView {
    LocationSnapshotView {
        location_id: snapshot.location_id,
        location_name: snapshot.location_name,
        capacity: snapshot.capacity,
        capacity_display: format_capacity(snapshot.capacity),
        estimated_occupants: snapshot.mobiles,
        capacity_status: capacity_status_view(snapshot.capacity, snapshot.mobiles),
        total: snapshot.total,
        personal: snapshot.personal,
        mobiles: snapshot.mobiles,
        laptops: snapshot.laptops,
        wearables: snapshot.wearables,
        captured_at: format_timestamp(Some(snapshot.time)),
    }
}

fn live_sample_view(sample: LocationLiveRow) -> LiveSampleView {
    LiveSampleView {
        location_name: sample.location_name,
        capacity: sample.capacity,
        capacity_display: format_capacity(sample.capacity),
        estimated_occupants: sample.mobiles,
        capacity_status: capacity_status_view(sample.capacity, sample.mobiles),
        device_name: sample.device_name,
        total: sample.total,
        personal: sample.personal,
        mobiles: sample.mobiles,
        laptops: sample.laptops,
        wearables: sample.wearables,
        unknowns: sample.unknowns,
        apples: sample.apples,
        googles: sample.googles,
        microsofts: sample.microsofts,
        samsungs: sample.samsungs,
        captured_at: format_timestamp(Some(sample.time)),
        captured_at_unix: sample.time,
    }
}

fn history_point_view(point: LocationHistoryRow) -> HistoryPointView {
    HistoryPointView {
        total: point.total,
        personal: point.personal,
        mobiles: point.mobiles,
        laptops: point.laptops,
        wearables: point.wearables,
        captured_at: format_timestamp(Some(point.time)),
        captured_at_unix: point.time,
    }
}

fn location_view(location: LocationOption, selected_location_id: Option<i64>) -> LocationView {
    LocationView {
        capacity: location.capacity,
        capacity_display: format_capacity(location.capacity),
        selected: selected_location_id == Some(location.location_id),
        location_id: location.location_id,
        location_name: location.location_name,
    }
}

fn format_capacity(capacity: Option<i64>) -> String {
    capacity
        .map(|value| value.to_string())
        .unwrap_or_else(|| "--".to_string())
}

fn capacity_status_view(capacity: Option<i64>, estimated_occupants: i64) -> CapacityStatusView {
    match capacity {
        Some(capacity) if estimated_occupants > capacity => CapacityStatusView {
            state: "over",
            label: "Over capacity".to_string(),
            detail: format!(
                "{} over the limit of {}",
                estimated_occupants - capacity,
                capacity
            ),
        },
        Some(capacity) => CapacityStatusView {
            state: "within",
            label: "Within capacity".to_string(),
            detail: format!(
                "{} spots remaining out of {}",
                capacity - estimated_occupants,
                capacity
            ),
        },
        None => CapacityStatusView {
            state: "unknown",
            label: "Capacity not set".to_string(),
            detail: "Add a fire-code limit for automated occupancy checks.".to_string(),
        },
    }
}

fn select_location(
    locations: &[LocationOption],
    requested_location_id: Option<i64>,
) -> Result<LocationOption, AppError> {
    if locations.is_empty() {
        return Err(AppError("No locations are configured".to_string()));
    }

    if let Some(location_id) = requested_location_id {
        if let Some(location) = locations
            .iter()
            .find(|location| location.location_id == location_id)
        {
            return Ok(location.clone());
        }
    }

    Ok(locations[0].clone())
}

fn format_timestamp(timestamp: Option<i64>) -> String {
    timestamp
        .and_then(|value| Local.timestamp_opt(value, 0).single())
        .map(|value| value.format("%Y-%m-%d %H:%M:%S").to_string())
        .unwrap_or_else(|| "No data yet".to_string())
}

#[derive(Debug)]
struct AppError(String);

impl From<sqlx::Error> for AppError {
    fn from(value: sqlx::Error) -> Self {
        Self(value.to_string())
    }
}

impl From<tera::Error> for AppError {
    fn from(value: tera::Error) -> Self {
        Self(value.to_string())
    }
}

impl IntoResponse for AppError {
    fn into_response(self) -> Response {
        (StatusCode::INTERNAL_SERVER_ERROR, self.0).into_response()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn renders_index_template() {
        let templates = load_templates().expect("templates should load");
        let page = DashboardPage {
            filters: DashboardFilterView {
                selected_device_name: String::new(),
                limit: 50,
            },
            summary: SummaryView {
                sample_count: 1,
                avg_total: "12.0".to_string(),
                max_total: 24,
                latest_seen: "2026-04-30 12:00:00".to_string(),
            },
            records: vec![RecordView {
                id: 1,
                location_name: "Rice Hall".to_string(),
                device_name: "test-device".to_string(),
                total: 24,
                personal: 20,
                mobiles: 18,
                laptops: 1,
                wearables: 1,
                unknowns: 0,
                apples: 10,
                googles: 5,
                microsofts: 2,
                samsungs: 3,
                captured_at: "2026-04-30 12:00:00".to_string(),
            }],
            locations: vec![LocationView {
                location_id: 1,
                location_name: "Rice Hall".to_string(),
                capacity: Some(15),
                capacity_display: "15".to_string(),
                selected: true,
            }],
            location_snapshots: vec![LocationSnapshotView {
                location_id: 1,
                location_name: "Rice Hall".to_string(),
                capacity: Some(15),
                capacity_display: "15".to_string(),
                estimated_occupants: 18,
                capacity_status: capacity_status_view(Some(15), 18),
                total: 24,
                personal: 20,
                mobiles: 18,
                laptops: 1,
                wearables: 1,
                captured_at: "2026-04-30 12:00:00".to_string(),
            }],
        };

        let mut context = Context::new();
        context.insert("page", &page);
        templates
            .render("index.html", &context)
            .expect("index template should render");
    }
}
