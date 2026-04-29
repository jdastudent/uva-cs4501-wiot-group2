use serde::Serialize;
use sqlx::{FromRow, Pool, Sqlite};

use crate::AppMessage;

const LOCATIONS: &[(u8, &str)] = &[
    (0x1, "Rice Hall"),
    (0x2, "Olsson Hall"),
    (0x3, "Thornton Hall"),
    (0x4, "Mech Eng Bldg"),
    (0x5, "Jesser Hall"),
    (0x6, "Chem Engr Bldg"),
    (0x7, "Wilsdorf Hall"),
    (0x8, "Physics Bldg"),
    (0x9, "Chemistry Bldg"),
    (0xa, "Life Sciences Bldg"),
    (0xb, "Gilmer Hall"),
    (0xc, "APMA Small Hall"),
    (0xd, "AFC"),
];

#[derive(Clone, Debug, Serialize, FromRow)]
pub struct LocationOption {
    pub location_id: i64,
    pub location_name: String,
}

#[derive(Clone, Debug, Serialize, FromRow)]
pub struct RecordRow {
    pub id: i64,
    pub location_name: String,
    pub device_name: String,
    pub total: i64,
    pub personal: i64,
    pub mobiles: i64,
    pub laptops: i64,
    pub wearables: i64,
    pub unknowns: i64,
    pub apples: i64,
    pub googles: i64,
    pub microsofts: i64,
    pub samsungs: i64,
    pub time: i64,
}

#[derive(Clone, Debug, Serialize, FromRow)]
pub struct SummaryRow {
    pub sample_count: i64,
    pub avg_total: f64,
    pub max_total: i64,
    pub latest_time: Option<i64>,
}

#[derive(Clone, Debug, Serialize, FromRow)]
pub struct LocationSnapshotRow {
    pub location_name: String,
    pub total: i64,
    pub personal: i64,
    pub mobiles: i64,
    pub laptops: i64,
    pub wearables: i64,
    pub time: i64,
}

#[derive(Clone, Debug, Serialize, FromRow)]
pub struct LocationLiveRow {
    pub location_id: i64,
    pub location_name: String,
    pub device_name: String,
    pub total: i64,
    pub personal: i64,
    pub mobiles: i64,
    pub laptops: i64,
    pub wearables: i64,
    pub unknowns: i64,
    pub apples: i64,
    pub googles: i64,
    pub microsofts: i64,
    pub samsungs: i64,
    pub time: i64,
}

#[derive(Clone, Debug, Serialize, FromRow)]
pub struct LocationHistoryRow {
    pub time: i64,
    pub total: i64,
    pub personal: i64,
    pub mobiles: i64,
    pub laptops: i64,
    pub wearables: i64,
}

pub async fn init_db(pool: &Pool<Sqlite>) {
    sqlx::query(
        "create table if not exists locations (
            location_id integer primary key,
            location_name text unique not null
        )",
    )
    .execute(pool)
    .await
    .unwrap();

    sqlx::query(
        "create table if not exists devices (
            device_id integer primary key,
            device_name text unique not null
        )",
    )
    .execute(pool)
    .await
    .unwrap();

    sqlx::query(
        "create table if not exists data (
            id integer primary key autoincrement,
            total integer not null,
            personal integer not null,
            mobiles integer not null,
            laptops integer not null,
            wearables integer not null,
            unknowns integer not null,
            apples integer not null,
            googles integer not null,
            microsofts integer not null,
            samsungs integer not null,
            location_id integer references locations(location_id),
            device_id integer references devices(device_id),
            time integer not null
        )",
    )
    .execute(pool)
    .await
    .unwrap();

    for &(id, location) in LOCATIONS {
        sqlx::query(
            "insert into locations (location_id, location_name)
             values ($1, $2)
             on conflict(location_id) do nothing",
        )
        .bind(id)
        .bind(location)
        .execute(pool)
        .await
        .unwrap();
    }
}

pub async fn insert_db(data: AppMessage, device_id: String, timestamp: i64, pool: &Pool<Sqlite>) {
    let id: i64 = sqlx::query_scalar(
        r#"insert into devices (device_name)
           values ($1)
           on conflict (device_name) do update set device_name = excluded.device_name
           returning device_id"#,
    )
    .bind(device_id)
    .fetch_one(pool)
    .await
    .unwrap();

    sqlx::query(
        r#"insert into data
           (total,
            personal,
            mobiles,
            laptops,
            wearables,
            unknowns,
            apples,
            googles,
            microsofts,
            samsungs,
            location_id,
            device_id,
            time)
           values ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13)"#,
    )
    .bind(data.total_devices)
    .bind(data.personal_devices)
    .bind(data.mobile_devices)
    .bind(data.laptops)
    .bind(data.wearables)
    .bind(data.unknowns)
    .bind(data.apple_devices)
    .bind(data.google_devices)
    .bind(data.microsoft_devices)
    .bind(data.samsung_devices)
    .bind(data.location_id)
    .bind(id)
    .bind(timestamp)
    .execute(pool)
    .await
    .unwrap();
}

pub async fn list_locations(pool: &Pool<Sqlite>) -> Result<Vec<LocationOption>, sqlx::Error> {
    sqlx::query_as::<_, LocationOption>(
        "select location_id, location_name
         from locations
         order by location_name asc",
    )
    .fetch_all(pool)
    .await
}

pub async fn fetch_records(
    pool: &Pool<Sqlite>,
    location_id: Option<i64>,
    device_name: Option<&str>,
    limit: i64,
) -> Result<Vec<RecordRow>, sqlx::Error> {
    sqlx::query_as::<_, RecordRow>(
        "select d.id,
                l.location_name,
                dv.device_name,
                d.total,
                d.personal,
                d.mobiles,
                d.laptops,
                d.wearables,
                d.unknowns,
                d.apples,
                d.googles,
                d.microsofts,
                d.samsungs,
                d.time
         from data d
         join locations l on l.location_id = d.location_id
         join devices dv on dv.device_id = d.device_id
         where (? is null or d.location_id = ?)
           and (? is null or dv.device_name = ?)
         order by d.time desc
         limit ?",
    )
    .bind(location_id)
    .bind(location_id)
    .bind(device_name)
    .bind(device_name)
    .bind(limit)
    .fetch_all(pool)
    .await
}

pub async fn fetch_summary(
    pool: &Pool<Sqlite>,
    location_id: Option<i64>,
    device_name: Option<&str>,
) -> Result<SummaryRow, sqlx::Error> {
    sqlx::query_as::<_, SummaryRow>(
        "select count(*) as sample_count,
                coalesce(avg(d.total), 0.0) as avg_total,
                coalesce(max(d.total), 0) as max_total,
                max(d.time) as latest_time
         from data d
         join devices dv on dv.device_id = d.device_id
         where (? is null or d.location_id = ?)
           and (? is null or dv.device_name = ?)",
    )
    .bind(location_id)
    .bind(location_id)
    .bind(device_name)
    .bind(device_name)
    .fetch_one(pool)
    .await
}

pub async fn fetch_latest_location_snapshots(
    pool: &Pool<Sqlite>,
) -> Result<Vec<LocationSnapshotRow>, sqlx::Error> {
    sqlx::query_as::<_, LocationSnapshotRow>(
        "select l.location_name,
                d.total,
                d.personal,
                d.mobiles,
                d.laptops,
                d.wearables,
                d.time
         from data d
         join locations l on l.location_id = d.location_id
         where d.id in (
             select d2.id
             from data d2
             where d2.location_id = d.location_id
             order by d2.time desc, d2.id desc
             limit 1
         )
         order by d.time desc, l.location_name asc",
    )
    .fetch_all(pool)
    .await
}

pub async fn fetch_latest_for_location(
    pool: &Pool<Sqlite>,
    location_id: i64,
) -> Result<Option<LocationLiveRow>, sqlx::Error> {
    sqlx::query_as::<_, LocationLiveRow>(
        "select d.location_id,
                l.location_name,
                dv.device_name,
                d.total,
                d.personal,
                d.mobiles,
                d.laptops,
                d.wearables,
                d.unknowns,
                d.apples,
                d.googles,
                d.microsofts,
                d.samsungs,
                d.time
         from data d
         join locations l on l.location_id = d.location_id
         join devices dv on dv.device_id = d.device_id
         where d.location_id = ?
         order by d.time desc, d.id desc
         limit 1",
    )
    .bind(location_id)
    .fetch_optional(pool)
    .await
}

pub async fn fetch_recent_location_history(
    pool: &Pool<Sqlite>,
    location_id: i64,
    limit: i64,
) -> Result<Vec<LocationHistoryRow>, sqlx::Error> {
    sqlx::query_as::<_, LocationHistoryRow>(
        "select time, total, personal, mobiles, laptops, wearables
         from (
             select d.time,
                    d.total,
                    d.personal,
                    d.mobiles,
                    d.laptops,
                    d.wearables
             from data d
             where d.location_id = ?
             order by d.time desc, d.id desc
             limit ?
         )
         order by time asc",
    )
    .bind(location_id)
    .bind(limit)
    .fetch_all(pool)
    .await
}
