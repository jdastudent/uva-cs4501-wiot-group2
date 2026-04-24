use std::{env, future::pending, sync::Arc};

use base64::{prelude::BASE64_STANDARD, Engine};
use chrono::{DateTime, FixedOffset, Local};
use dotenvy::dotenv;
use mqtt5::{ConnectOptions, Message, MqttClient};
use serde::Deserialize;
use serde_json::from_str;
use sqlx::{sqlite::SqlitePoolOptions, Pool, Sqlite};

use crate::{
    db::{init_db, insert_db},
    parse::parse_app_message,
};

mod db;
pub mod parse;

const TOPIC: &'static str = "v3/+/devices/+/up";

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    dotenv().expect(".env should contain credentials");

    let address = env::var("TTN_BROKER").expect("broker should be set in .env");
    let username = env::var("TTN_USERNAME").expect("username should be set in .env");
    let password = env::var("TTN_PASSWORD").expect("password should be set in .env");

    println!("Connecting to mqtt server...");
    let client = MqttClient::with_options(
        ConnectOptions::new("final-proj-listener")
            .with_protocol_version(mqtt5::ProtocolVersion::V311)
            .with_credentials(username, password), // .with_clean_start(true),
    );

    // Multiple transport options:
    client.connect(&format!("mqtts://{address}:8883")).await?; // TCP

    println!("Connecting to database...");
    let pool = Arc::new(
        SqlitePoolOptions::new()
            .max_connections(5)
            .connect("sqlite://database.db")
            .await
            .expect("Database file exists"),
    );
    println!("Creating tables...");
    init_db(&*pool).await;

    println!("Subscribing to endpoint...");
    // Subscribe with callback
    client
        .subscribe(TOPIC, move |x| {
            let pool = pool.clone();
            tokio::spawn(async move {
                recieve_message(x, pool.clone()).await;
            });
        })
        .await?;

    println!("Listenening");
    // Publish a message
    //
    // client.publish("sensors/temp/data", b"25.5 C").await?;

    // tokio::time::sleep(tokio::time::Duration::from_secs(60 * 5)).await;
    pending::<()>().await;
    Ok(())
}

#[derive(Deserialize)]
struct Data {
    uplink_message: UplinkMessage,
    end_device_ids: EndDeviceIds,
    received_at: DateTime<Local>,
}

#[derive(Deserialize)]
struct UplinkMessage {
    frm_payload: String,
}

#[derive(Deserialize)]
struct EndDeviceIds {
    device_id: String,
}

#[derive(Deserialize, Debug)]
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
impl AppMessage {
    pub fn get_checksum(&self) -> u8 {
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
        checksum ^= self.location_id as u8 & 0xFF;

        checksum
    }
}

async fn recieve_message(message: Message, pool: Arc<Pool<Sqlite>>) {
    // println!("recieved payload:");
    let payload = String::from_utf8_lossy(&message.payload);
    // println!("{payload}");
    let res: Data = from_str(&payload).expect("Properly formatted message");
    println!("Encoded data: {}", res.uplink_message.frm_payload);
    let data = BASE64_STANDARD
        .decode(res.uplink_message.frm_payload)
        .unwrap();
    let d_id = res.end_device_ids.device_id;
    let (_, message) = parse_app_message(&data).expect("Valid message data");
    if message.xor_checksum == message.get_checksum() {
        println!("Got message from '{d_id}' with proper checksum! : {message:?}");
        insert_db(message, d_id, res.received_at.timestamp(), &*pool).await;
    } else {
        println!("Got message from '{d_id}' with invalid checksum! : {message:?}");
    }
}
