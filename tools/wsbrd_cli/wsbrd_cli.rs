/*
 * Copyright (c) 2021-2023 Silicon Laboratories Inc. (www.silabs.com)
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at [1].  This software is distributed to you in
 * Object Code format and/or Source Code format and is governed by the sections
 * of the MSLA applicable to Object Code, Source Code and Modified Open Source
 * Code. By using this software, you agree to the terms of the MSLA.
 *
 * [1]: https://www.silabs.com/about-us/legal/master-software-license-agreement
 */
mod wsbrddbusapi;

extern crate dbus;
extern crate clap;

use std::time::Duration;
use dbus::blocking::Connection;
use dbus::arg::PropMap;
use dbus::arg::prop_cast;
use wsbrddbusapi::ComSilabsWisunBorderRouter;
////use clap::App;
use clap::AppSettings;
////use clap::SubCommand;
use clap::{Arg, App, SubCommand};
use std::net::{Ipv6Addr};
use std::string::String;


fn format_byte_array(input: &[u8]) -> String {
    input.iter().map(|m| format!("{:02x}", m)).collect::<Vec<_>>().join(":")
}

fn is_parent(node: &(Vec<u8>, PropMap), target: &[u8]) -> bool {
    let parent: Option<&Vec<u8>> = prop_cast(&node.1, "parent");
    match parent {
        Some(x) if AsRef::<[u8]>::as_ref(x) == target => true,
        Some(_) => false,
        None => false,
    }
}

fn is_border_router(node: &(Vec<u8>, PropMap)) -> bool {
    let is_br: Option<&bool> = prop_cast(&node.1, "is_border_router");
    match is_br {
        Some(&x) => x,
        None => false,
    }
}

fn print_rpl_tree(links: &[(Vec<u8>, PropMap)], parents: &[Vec<u8>], cur: &[u8], indent: &str) -> () {
    let mut children: Vec<_> = links.iter().filter(|n| is_parent(n, cur)).map(|n| &n.0).collect();
    children.sort();
    let mut new_parents = parents.to_vec();
    new_parents.push(cur.to_vec());
    if let Some((last_child, first_childs)) = children.split_last() {
        for c in first_childs {
            if new_parents.contains(c) {
                println!("{}|- {} (loop!)", indent, format_byte_array(c));
            } else {
                println!("{}|- {}", indent, format_byte_array(c));
                print_rpl_tree(links, &new_parents, c, &(indent.to_owned() + "|    "));
            }
        }
        if new_parents.contains(last_child) {
            println!("{}`- {} (loop!)", indent, format_byte_array(last_child));
        } else {
            println!("{}`- {}", indent, format_byte_array(last_child));
            print_rpl_tree(links, &new_parents, last_child, &(indent.to_owned() + "     "));
        }
    }
}

fn do_status(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    // Consider that if NetworkName does not exist, the service probably not here.
    match dbus_proxy.wisun_network_name() {
        Ok(val) => println!("network_name: {}", val),
        Err(e) => return Err(Box::new(e)),
    }

    match dbus_proxy.wisun_fan_version().unwrap_or(u8::MAX) {
        1 => println!("fan_version: FAN 1.0"),
        2 => println!("fan_version: FAN 1.1"),
        _ => (),
    }
    println!("domain: {}", dbus_proxy.wisun_domain().unwrap_or("[UNKNOWN]".to_string()));
    let mode         = dbus_proxy.wisun_mode().unwrap_or(0);
    let class        = dbus_proxy.wisun_class().unwrap_or(0);
    let phy_mode_id  = dbus_proxy.wisun_phy_mode_id().unwrap_or(0);
    let chan_plan_id = dbus_proxy.wisun_chan_plan_id().unwrap_or(0);
    if mode != 0 && class != 0 {
        println!("mode: {:x}", mode);
        println!("class: {}", class);
    } else if phy_mode_id != 0 && chan_plan_id != 0 {
        println!("phy_mode_id: {}", phy_mode_id);
        println!("chan_plan_id: {}", chan_plan_id);
    }
    println!("panid: {:#04x}", dbus_proxy.wisun_pan_id().unwrap_or(0));
    println!("size: {}", dbus_proxy.wisun_size().unwrap_or("[UNKNOWN]".to_string()));

    let gaks = dbus_proxy.gaks().unwrap_or(vec![]);
    for (i, g) in gaks.iter().enumerate() {
        println!("GAK[{}]: {}", i, format_byte_array(g));
    }

    let gtks = dbus_proxy.gtks().unwrap_or(vec![]);
    for (i, g) in gtks.iter().enumerate() {
        println!("GTK[{}]: {}", i, format_byte_array(g));
    }

    let lgaks = dbus_proxy.lgaks().unwrap_or(vec![]);
    for (i, g) in lgaks.iter().enumerate() {
        println!("LGAK[{}]: {}", i, format_byte_array(g));
    }

    let lgtks = dbus_proxy.lgtks().unwrap_or(vec![]);
    for (i, g) in lgtks.iter().enumerate() {
        println!("LGTK[{}]: {}", i, format_byte_array(g));
    }

    let nodes = dbus_proxy.nodes().unwrap();
    let node_br = nodes.iter().find(|n| is_border_router(n)).unwrap();
    println!("{}", format_byte_array(&node_br.0));
    print_rpl_tree(&nodes, &vec![], &node_br.0, "  ");
    Ok(())
}

fn do_stopfan10(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    match dbus_proxy.wisun_network_name() {
        Ok(val) => println!("stop FAN1.0: {}", val),
        Err(e) => return Err(Box::new(e)),
    }

    let vec: Vec<u8> = Vec::new();
    let _ret = dbus_proxy.stop_fan10(vec, 1);

    Ok(())
}

fn do_startfan10(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    match dbus_proxy.wisun_network_name() {
        Ok(val) => println!("Start FAN1.0: {}", val),
        Err(e) => return Err(Box::new(e)),
    }

    let vec: Vec<u8> = Vec::new();
    let _ret = dbus_proxy.start_fan10(vec, 1);

    Ok(())
}

fn get_networkstate(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("Network state:");
    let network_state = dbus_proxy.get_network_state().unwrap_or(vec![]);
    for (i, g) in network_state.iter().enumerate() {
        let ip_addr = Ipv6Addr::new((g[0] as u16)*256+(g[1] as u16), (g[2] as u16)*256+(g[3] as u16), 
                                    (g[4] as u16)*256+(g[5] as u16), (g[6] as u16)*256+(g[7] as u16), 
                                    (g[8] as u16)*256+(g[9] as u16), (g[10] as u16)*256+(g[11] as u16),
                                    (g[12] as u16)*256+(g[13] as u16), (g[14] as u16)*256+(g[15] as u16));
        println!("IP address[{}]: {}", i, ip_addr);
    }

    Ok(())
}

fn get_networkname(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    match dbus_proxy.wisun_network_name() {
        Ok(val) => println!("Wisun Network Name: {}", val),
        Err(e) => return Err(Box::new(e)),
    }

    Ok(())
}

fn get_wisun_phy_configs(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    match dbus_proxy.wisun_fan_version().unwrap_or(u8::MAX) {
        1 => println!("fan_version: FAN 1.0"),
        2 => println!("fan_version: FAN 1.1"),
        _ => (),
    }
    println!("domain: {}", dbus_proxy.wisun_domain().unwrap_or("[UNKNOWN]".to_string()));
    let mode         = dbus_proxy.wisun_mode().unwrap_or(0);
    let class        = dbus_proxy.wisun_class().unwrap_or(0);
    let phy_mode_id  = dbus_proxy.wisun_phy_mode_id().unwrap_or(0);
    let chan_plan_id = dbus_proxy.wisun_chan_plan_id().unwrap_or(0);
    if mode != 0 && class != 0 {
        println!("mode: {:x}", mode);
        println!("class: {}", class);
    } else if phy_mode_id != 0 && chan_plan_id != 0 {
        println!("phy_mode_id: {}", phy_mode_id);
        println!("chan_plan_id: {}", chan_plan_id);
    }

    Ok(())
}

fn set_networkname(dbus_user: bool, arg0: String) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("wisun network name setting: {}", arg0);
    let _ret = dbus_proxy.set_network_name(arg0);

    Ok(())
}

fn set_wisun_phy_configs(dbus_user: bool, arg0: u8, arg1: u8, arg2: u8) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("Wisun PHY config setting:: \nDomain: {}\nClass: {}\nMode: {}", arg0, arg1, arg2);
    let _ret = dbus_proxy.set_wisun_phy_configs(arg0, arg1, arg2);

    Ok(())
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut wisun_nwkname: String = String::from("Wi-SUN test");
    let mut wisun_domain: u8    = 0;
    let mut wisun_class: u8     = 0;
    let mut wisun_mode: u8      = 0;
    let matches = App::new("wsbrd_cli")
        .setting(AppSettings::SubcommandRequired)
        .args_from_usage("--user 'Use user bus instead of system bus'")
        .subcommand(SubCommand::with_name("status").about("Display a brief status of the Wi-SUN network"),)
        .subcommand(SubCommand::with_name("start-fan10").about("Start runing FAN1.0 BBR"),)
        .subcommand(SubCommand::with_name("stop-fan10").about("Stop the current runing FAN1.0 BBR"),)
        .subcommand(SubCommand::with_name("get-network-state").about("Show wisun network state"),)
        .subcommand(SubCommand::with_name("get-network-name").about("Show wisun network name"),)
        .subcommand(SubCommand::with_name("get-wisun-phy-configs").about("Show wisun phy configs"),)
        .subcommand(SubCommand::with_name("set-network-name").about("Set wisun network name. After set, the BBR will restart FAN")
            .arg(Arg::with_name("nwk_name").help("set expected wisun network name").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-wisun-phy-configs").about("Set wisun phy configs: domain, class and mode")
            .arg(Arg::with_name("domain").help("set expected wisun domain").empty_values(false))
            .arg(Arg::with_name("class").help("set expected wisun class").empty_values(false))
            .arg(Arg::with_name("mode").help("set expected wisun mode").empty_values(false))
        ,)
        .get_matches();
    let dbus_user = matches.is_present("user");

    match matches.subcommand_name() {
        Some("status")                  => do_status(dbus_user),
        Some("start-fan10")             => do_startfan10(dbus_user),
        Some("stop-fan10")              => do_stopfan10(dbus_user),
        Some("get-network-state")       => get_networkstate(dbus_user),
        Some("get-network-name")        => get_networkname(dbus_user),
        Some("get-wisun-phy-configs")   => get_wisun_phy_configs(dbus_user),
        Some("set-network-name")        => {
            if let Some(subcmd) = matches.subcommand_matches("set-network-name") {
                if let Some(nwkname) = subcmd.value_of("nwk_name") {
                    wisun_nwkname = nwkname.to_string();
                }
            }
            set_networkname(dbus_user, wisun_nwkname)
        }
        Some("set-wisun-phy-configs")   => {
            if let Some(subcmd) = matches.subcommand_matches("set-wisun-phy-configs") {
                if let Some(domainval) = subcmd.value_of("domain") {
                    wisun_domain = domainval.parse::<u8>().unwrap();
                }
                if let Some(domainval) = subcmd.value_of("class") {
                    wisun_class = domainval.parse::<u8>().unwrap();
                }
                if let Some(domainval) = subcmd.value_of("mode") {
                    wisun_mode = domainval.parse::<u8>().unwrap();
                }
            }
            set_wisun_phy_configs(dbus_user, wisun_domain, wisun_class, wisun_mode)
        }
        _ => Ok(()), // Already covered by AppSettings::SubcommandRequired
    }

}
