use nom::{
    bytes::complete::take,
    number::{be_u16, u16, u8, Endianness::Big},
    IResult, Parser,
};

use crate::AppMessage;

pub fn parse_app_message(i: &[u8]) -> IResult<&[u8], AppMessage> {
    let (rest, total_devices) = be_u16().parse(i)?;
    let (rest, personal_devices) = be_u16().parse(rest)?;
    let (rest, mobile_devices) = be_u16().parse(rest)?;
    let (rest, laptops) = be_u16().parse(rest)?;
    let (rest, wearables) = be_u16().parse(rest)?;
    let (rest, unknowns) = be_u16().parse(rest)?;
    let (rest, apple_devices) = be_u16().parse(rest)?;
    let (rest, google_devices) = be_u16().parse(rest)?;
    let (rest, microsoft_devices) = be_u16().parse(rest)?;
    let (rest, samsung_devices) = be_u16().parse(rest)?;
    let (rest, location_id) = u8().parse(rest)?;
    let (rest, xor_checksum) = u8().parse(rest)?;
    Ok((
        rest,
        AppMessage {
            total_devices,
            personal_devices,
            mobile_devices,
            laptops,
            wearables,
            unknowns,
            apple_devices,
            google_devices,
            microsoft_devices,
            samsung_devices,
            location_id,
            xor_checksum,
        },
    ))
}
