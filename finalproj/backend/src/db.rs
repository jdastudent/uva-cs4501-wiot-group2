use sqlx::{Pool, Sqlite};

use crate::AppMessage;

const LOCATIONS: &[(u8, &'static str)] = &[
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
    // TODO init locations in location table
    for &(id, location) in LOCATIONS {
        sqlx::query("INSERT INTO locations (location_id, location_name) VALUES ($1, $2) ON CONFLICT(location_id) DO NOTHING").bind(id).bind(location).execute(pool).await.unwrap();
    }
}

pub async fn insert_db(data: AppMessage, device_id: String, timestamp: i64, pool: &Pool<Sqlite>) {
    let id:i64 = sqlx::query_scalar(
        r#"INSERT INTO devices (device_name) VALUES ($1) ON CONFLICT (device_name) DO UPDATE SET device_name = EXCLUDED.device_name RETURNING device_id"#,
    )
    .bind(device_id)
    .fetch_one(pool)
    .await.unwrap();

    sqlx::query(
        r#"INSERT INTO data
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
        VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13)"#,
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
