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

    println!("--------------------------------------------------------------");
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

    println!("--------------------------------------------------------------");
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

    println!("--------------------------------------------------------------");
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
    println!("--------------------------------------------------------------");
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

    println!("--------------------------------------------------------------");
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
    println!("--------------------------------------------------------------");
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

fn get_timing_parameters(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("Timing parameters:");
    println!("--------------------------------------------------------------");
    let timing_parameters = dbus_proxy.get_timing_parameters().unwrap_or(vec![]);
    println!("disc_trickle_imin:\t {}", timing_parameters[0]);
    println!("disc_trickle_imax:\t {}", timing_parameters[1]);
    println!("disc_trickle_k:   \t {}", timing_parameters[2]);
    println!("pan_timeout:      \t {}", timing_parameters[3]);

    Ok(())
}

fn get_fhss_channel_mask(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("Fhss channel mask:");
    println!("--------------------------------------------------------------");
    let fhss_channel_mask = dbus_proxy.get_fhss_channel_mask().unwrap_or(vec![]);
    println!("{:#08x?}", fhss_channel_mask);

    Ok(())
}

fn get_fhss_timing_configure(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("Fhss timing configure:");
    println!("--------------------------------------------------------------");
    let fhss_timing_configure = dbus_proxy.get_fhss_timing_configure().unwrap_or(vec![]);
    println!("uc_dwell_interval:  {}", fhss_timing_configure[0]);
    println!("broadcast_interval: {}", fhss_timing_configure[1]);
    println!("bc_dwell_interval:  {}", fhss_timing_configure[2]);
    println!("uc_channel_function:{}", fhss_timing_configure[3]);
    println!("bc_channel_function:{}", fhss_timing_configure[4]);

    Ok(())
}

fn get_wisun_pan_id(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Wi-SUN PAN id: {:#04x}", dbus_proxy.wisun_pan_id().unwrap_or(0));

    Ok(())
}

fn get_wisun_pan_size(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Wi-SUN PAN size: {}", dbus_proxy.wisun_size().unwrap_or("[UNKNOWN]".to_string()));

    Ok(())
}

fn get_wisun_gtks(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    let gtks = dbus_proxy.gtks().unwrap_or(vec![]);
    println!("Wi-SUN GTKs:");
    println!("--------------------------------------------------------------");
    for (i, g) in gtks.iter().enumerate() {
        println!("GTK[{}]: {}", i, format_byte_array(g));
    }

    Ok(())
}

fn get_wisun_gtk_active_key_index(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Wi-SUN GTK active key index: {}", dbus_proxy.get_gtk_active_key_index().unwrap_or(0));

    Ok(())
}

fn get_wisun_cfg_settings(dbus_user: bool) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("Wi-SUN configuration settings:");
    println!("-----------------------------------------------------------------");
    match dbus_proxy.wisun_network_name() {
        Ok(val) => println!("Wisun Network Name                  :{}", val),
        Err(e) => return Err(Box::new(e)),
    }

    let wisun_cfg_settings = dbus_proxy.get_wisun_cfg_settings().unwrap_or(vec![]);
    println!("ws_cfg_gen_network_size                   :{}",      wisun_cfg_settings[0]);
    println!("ws_cfg_gen_network_pan_id                 :{:#04x}", wisun_cfg_settings[1]);
    println!("ws_cfg_gen_rpl_parent_candidate_max       :{}", wisun_cfg_settings[2]);
    println!("ws_cfg_gen_rpl_selected_parent_max        :{}", wisun_cfg_settings[3]);

    println!("ws_cfg_phy_regulatory_domain              :{}", wisun_cfg_settings[4]);
    println!("ws_cfg_phy_operating_class                :{}", wisun_cfg_settings[5]);
    println!("ws_cfg_phy_operating_mode                 :{}", wisun_cfg_settings[6]);
    println!("ws_cfg_phy_phy_mode_id                    :{}", wisun_cfg_settings[7]);
    println!("ws_cfg_phy_channel_plan_id                :{}", wisun_cfg_settings[8]);

    println!("ws_cfg_timing_disc_trickle_imin           :{}", wisun_cfg_settings[9]);
    println!("ws_cfg_timing_disc_trickle_imax           :{}", wisun_cfg_settings[10]);
    println!("ws_cfg_timing_disc_trickle_k              :{}", wisun_cfg_settings[11]);
    println!("ws_cfg_timing_pan_timeout                 :{}", wisun_cfg_settings[12]);
    println!("ws_cfg_timing_temp_link_min_timeout       :{}", wisun_cfg_settings[13]);
    println!("ws_cfg_timing_temp_eapol_min_timeout      :{}", wisun_cfg_settings[14]);

    println!("ws_cfg_bbr_dio_interval_min               :{}", wisun_cfg_settings[15]);
    println!("ws_cfg_bbr_dio_interval_doublings         :{}", wisun_cfg_settings[16]);
    println!("ws_cfg_bbr_dio_redundancy_constant        :{}", wisun_cfg_settings[17]);
    println!("ws_cfg_bbr_dag_max_rank_increase          :{}", wisun_cfg_settings[18]);
    println!("ws_cfg_bbr_min_hop_rank_increase          :{}", wisun_cfg_settings[19]);
    println!("ws_cfg_bbr_rpl_default_lifetime           :{}", (wisun_cfg_settings[20] as u32) + (wisun_cfg_settings[21] as u32)*65536);

    println!("ws_cfg_fhss_uc_dwell_interval             :{}", wisun_cfg_settings[22]);
    println!("ws_cfg_fhss_bc_dwell_interval             :{}", wisun_cfg_settings[23]);
    println!("ws_cfg_fhss_bc_interval                   :{}", (wisun_cfg_settings[24] as u32) + (wisun_cfg_settings[25] as u32)*65526);
    println!("ws_cfg_fhss_uc_channel_function           :{}", wisun_cfg_settings[26]);
    println!("ws_cfg_fhss_uc_fixed_channel              :{}", wisun_cfg_settings[27]);
    println!("ws_cfg_fhss_bc_fixed_channel              :{}", wisun_cfg_settings[28]);
    println!("ws_cfg_fhss_channel_mask[0]               :{}", wisun_cfg_settings[29]);
    println!("ws_cfg_fhss_channel_mask[1]               :{}", wisun_cfg_settings[30]);
    println!("ws_cfg_fhss_channel_mask[2]               :{}", wisun_cfg_settings[31]);
    println!("ws_cfg_fhss_channel_mask[3]               :{}", wisun_cfg_settings[32]);
    println!("ws_cfg_fhss_channel_mask[4]               :{}", wisun_cfg_settings[33]);
    println!("ws_cfg_fhss_channel_mask[5]               :{}", wisun_cfg_settings[34]);
    println!("ws_cfg_fhss_channel_mask[6]               :{}", wisun_cfg_settings[35]);
    println!("ws_cfg_fhss_channel_mask[7]               :{}", wisun_cfg_settings[36]);

    println!("ws_cfg_mpl_trickle_imin                   :{}", wisun_cfg_settings[37]);
    println!("ws_cfg_mpl_trickle_imax                   :{}", wisun_cfg_settings[38]);
    println!("ws_cfg_mpl_trickle_k                      :{}", wisun_cfg_settings[39]);
    println!("ws_cfg_mpl_trickle_timer_exp              :{}", wisun_cfg_settings[40]);
    println!("ws_cfg_mpl_seed_set_entry_lifetime        :{}", wisun_cfg_settings[41]);

    println!("ws_cfg_sectimer_gtk_expire_offset         :{}", (wisun_cfg_settings[42] as u32) + (wisun_cfg_settings[43] as u32)*65536);
    println!("ws_cfg_sectimer_pmk_lifetime              :{}", (wisun_cfg_settings[44] as u32) + (wisun_cfg_settings[45] as u32)*65536);
    println!("ws_cfg_sectimer_ptk_lifetime              :{}", (wisun_cfg_settings[46] as u32) + (wisun_cfg_settings[47] as u32)*65536);
    println!("ws_cfg_sectimer_gtk_new_act_time          :{}", wisun_cfg_settings[48]);
    println!("ws_cfg_sectimer_revocat_lifetime_reduct   :{}",wisun_cfg_settings[49]);
    println!("ws_cfg_sectimer_gtk_new_install_req       :{}", wisun_cfg_settings[50]);

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

    println!("--------------------------------------------------------------");
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

    println!("--------------------------------------------------------------");
    println!("Wisun PHY config setting: \nDomain: {}\nClass: {}\nMode: {}", arg0, arg1, arg2);
    let _ret = dbus_proxy.set_wisun_phy_configs(arg0, arg1, arg2);

    Ok(())
}

fn set_timing_parameters(dbus_user: bool, arg0: u16, arg1: u16, arg2: u8, arg3: u16) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Set new timing parameters: \ntrickle_imin: {}\ntrickle_imax: {}\ntrickle_k: {}\npan_timeout: {}", arg0, arg1, arg2, arg3);
    let _ret = dbus_proxy.set_timing_parameters(arg0, arg1, arg2, arg3);

    Ok(())
}

fn set_fhss_channel_mask_f4b(dbus_user: bool, arg0: u32, arg1: u32, arg2: u32, arg3: u32) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Set First 4 long word Channel mask: \nChannel_mask[0]: {:#08x}\nChannel_mask[1]: {:#08x}\nChannel_mask[0]: {:#08x}\nChannel_mask[0]: {:#08x}", arg0, arg1, arg2, arg3);
    let _ret = dbus_proxy.set_fhss_channel_mask_f4b(arg0, arg1, arg2, arg3);

    Ok(())
}

fn set_fhss_channel_mask_l4b(dbus_user: bool, arg0: u32, arg1: u32, arg2: u32, arg3: u32) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Set Last 4 long word Channel mask: \nChannel_mask[4]: {:#08x}\nChannel_mask[5]: {:#08x}\nChannel_mask[6]: {:#08x}\nChannel_mask[7]: {:#08x}", arg0, arg1, arg2, arg3);
    let _ret = dbus_proxy.set_fhss_channel_mask_l4b(arg0, arg1, arg2, arg3);

    Ok(())
}

fn set_fhss_timing_configure(dbus_user: bool, arg0: u8, arg1: u32, arg2: u8) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Set fhss timing configure: \nuc_dwell_interval: {}\nbroadcast_interval: {}\nbc_dwell_interval: {}", arg0, arg1, arg2);
    let _ret = dbus_proxy.set_fhss_timing_configure(arg0, arg1, arg2);

    Ok(())
}

fn set_fhss_uc_function(dbus_user: bool, arg0: u8, arg1: u16, arg2: u8) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Set fhss unicast configure: \nchannel_function: {}\nfixed_channel: {}\ndwell_interval: {}", arg0, arg1, arg2);
    let _ret = dbus_proxy.set_fhss_uc_function(arg0, arg1, arg2);

    Ok(())
}

fn set_fhss_bc_function(dbus_user: bool, arg0: u8, arg1: u16, arg2: u8, arg3: u32) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Set fhss broadcast configure: \nchannel_function: {}\nfixed_channel: {}\ndwell_interval: {}\nbroadcast_interval: {}", arg0, arg1, arg2, arg3);
    let _ret = dbus_proxy.set_fhss_bc_function(arg0, arg1, arg2, arg3);

    Ok(())
}

fn set_wisun_pan_id(dbus_user: bool, arg0: u16) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Set Wi-SUN PAN id: {:#04x}", arg0);
    let _ret = dbus_proxy.set_wisun_pan_id(arg0);

    Ok(())
}

fn set_wisun_pan_size(dbus_user: bool, arg0: u16) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Set Wi-SUN PAN size: {:#04x}", arg0);
    let pan_size = arg0;
    if !(pan_size == 0 || pan_size == 1 || pan_size == 8 || pan_size == 15 || pan_size == 25 || pan_size == 255) {
        println!("Invalid pan_size setting input");
        println!("Valid pan_size setting is: \n0:\tNetwork configuration used in Wi-SUN certification\n1:\tsmall, \n8:\tmedium(100~800 devices), \n15:\tlarge(800~1500 devices), \n25:\textreme large(<2500 devices), \n255:\tauto");
    } else {
        let _ret = dbus_proxy.set_wisun_pan_size(arg0);
    }

    Ok(())
}

fn set_wisun_gtk_key(dbus_user: bool, arg0: String) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("input GTK kes: {:?}", arg0);
    let gtk_keys: Vec<u8> = arg0.to_string()
        .split(":")
        .map(|s| s.parse().expect("parse error"))
        .collect();

    let _ret = dbus_proxy.set_wisun_gtk_key(gtk_keys);

    Ok(())
}

fn set_wisun_gtk_active_key(dbus_user: bool, arg0: u8) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Set Wi-SUN GTK active key: {}", arg0);
    let gtk_index = arg0;
    if gtk_index >3 {
        println!("Invalid GTK active index setting. Valid range is [0, 3]");
    } else {
        let _ret = dbus_proxy.set_wisun_gtk_active_key(arg0);
    }

    Ok(())
}

fn set_wisun_key_lifetime(dbus_user: bool, arg0: u32, arg1: u32, arg2: u32) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Set Wi-SUN key lifetime: \ngtk_lifetime: {}\npmk_lifetime: {}\nptk_lifetime: {}", arg0, arg1, arg2);
    let _ret = dbus_proxy.set_wisun_key_lifetime(arg0, arg1, arg2);

    Ok(())
}

fn create_udp_socket(dbus_user: bool, arg0: u16) -> Result<(), Box<dyn std::error::Error>> {
    let dbus_conn;
    if dbus_user {
        dbus_conn = Connection::new_session()?;
    } else {
        dbus_conn = Connection::new_system()?;
    }
    let dbus_proxy = dbus_conn.with_proxy("com.silabs.Wisun.BorderRouter", "/com/silabs/Wisun/BorderRouter", Duration::from_millis(500));

    println!("--------------------------------------------------------------");
    println!("Create UDP socket: {:?}", arg0);
    let _ret = dbus_proxy.create_udp_socket(arg0);

    Ok(())
}



fn main() -> Result<(), Box<dyn std::error::Error>> {
    let mut wisun_nwkname: String   = String::from("Wi-SUN test");
    let mut wisun_domain: u8        = 0;
    let mut wisun_class: u8         = 0;
    let mut wisun_mode: u8          = 0;
    let mut trickle_imin: u16       = 0;
    let mut trickle_imax: u16       = 0;
    let mut trickle_k: u8           = 0;
    let mut pan_timeout: u16        = 0;
    let mut pan_id: u16             = 0;
    let mut pan_size: u16           = 0;
    let mut long_word0: u32         = 0xffffffff;
    let mut long_word1: u32         = 0xffffffff;
    let mut long_word2: u32         = 0xffffffff;
    let mut long_word3: u32         = 0xffffffff;
    let mut long_word4: u32         = 0xffffffff;
    let mut long_word5: u32         = 0xffffffff;
    let mut long_word6: u32         = 0xffffffff;
    let mut long_word7: u32         = 0xffffffff;
    let mut uc_dwell_interval: u8   = 0;
    let mut broadcast_interval: u32 = 0;
    let mut bc_dwell_interval:u8    = 0;
    let mut channel_function: u8    = 0;
    let mut fixed_channel: u16      = 0;
    let mut dwell_interval: u8      = 0;
    let mut gtk_key: String         = String::from("");
    let mut gtk_index: u8           = 0;
    let mut gtk_lifetime: u32       = 0xffffffff;
    let mut pmk_lifetime: u32       = 0xffffffff;
    let mut ptk_lifetime: u32       = 0xffffffff;
    let mut udp_port: u16           = 0;

    let matches = App::new("wsbrd_cli")
        .setting(AppSettings::SubcommandRequired)
        .args_from_usage("--user 'Use user bus instead of system bus'")
        .subcommand(SubCommand::with_name("status").about("Display a brief status of the Wi-SUN network"),)
        .subcommand(SubCommand::with_name("start-fan10").about("Start runing FAN1.0 BBR"),)
        .subcommand(SubCommand::with_name("stop-fan10").about("Stop the current runing FAN1.0 BBR"),)
        .subcommand(SubCommand::with_name("get-network-state").about("Show wisun network state"),)
        .subcommand(SubCommand::with_name("get-network-name").about("Show wisun network name"),)
        .subcommand(SubCommand::with_name("get-wisun-phy-configs").about("Show wisun phy configs"),)
        .subcommand(SubCommand::with_name("get-timing-parameters").about("Get timing parameters, trickle_imin, trickle_imax, trickle_k: and pan_timeout"),)
        .subcommand(SubCommand::with_name("get-fhss-channel-mask").about("Get fhss channel mask array[8]"),)
        .subcommand(SubCommand::with_name("get-fhss-timing-configure").about("Get fhss timing configure such as bc_dwell_interval bc_interval bc_dwell_interval uc_channel_function bc_channel_function"),)
        .subcommand(SubCommand::with_name("get-wisun-pan-id").about("Get Wi-SUN PAN ID"),)
        .subcommand(SubCommand::with_name("get-wisun-pan-size").about("Get Wi-SUN PAN size"),)
        .subcommand(SubCommand::with_name("get-wisun-gtk-keys").about("Get wisun index gtk keys"),)
        .subcommand(SubCommand::with_name("get-wisun-gtk-active-key-index").about("Get wisun gtk active key index"),)
        .subcommand(SubCommand::with_name("get-wisun-cfg-settings").about("Get wisun configuration settings"),)
        .subcommand(SubCommand::with_name("set-network-name").about("Set wisun network name. After set, the BBR will restart FAN")
            .arg(Arg::with_name("nwk_name").help("set expected wisun network name").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-wisun-phy-configs").about("Set wisun phy configs: domain, class and mode")
            .arg(Arg::with_name("domain").help("set expected wisun domain").empty_values(false))
            .arg(Arg::with_name("class").help("set expected wisun class").empty_values(false))
            .arg(Arg::with_name("mode").help("set expected wisun mode").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-timing-parameters").about("Set timing parameters, trickle_imin, trickle_imax, trickle_k: and pan_timeout")
            .arg(Arg::with_name("trickle_imin").help("set expected minimum trickle interval").empty_values(false))
            .arg(Arg::with_name("trickle_imax").help("set expected maximum trickle interval").empty_values(false))
            .arg(Arg::with_name("trickle_k").help("set expected trickle k").empty_values(false))
            .arg(Arg::with_name("pan_timeout").help("set expected PAN timeout").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-fhss-channel-mask-f4b").about("Set first 4 long word(32bit) of fhss channel mask")
            .arg(Arg::with_name("long_word0").help("Set 1st long word(32bit) of fhss channel mask").empty_values(false))
            .arg(Arg::with_name("long_word1").help("Set 2nd long word(32bit) of fhss channel mask").empty_values(false))
            .arg(Arg::with_name("long_word2").help("Set 3rd long word(32bit) of fhss channel mask").empty_values(false))
            .arg(Arg::with_name("long_word3").help("Set 4th long word(32bit) of fhss channel mask").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-fhss-channel-mask-l4b").about("Set last 4 long word(32bit) of fhss channel mask")
            .arg(Arg::with_name("long_word4").help("Set 4st long word(32bit) of fhss channel mask").empty_values(false))
            .arg(Arg::with_name("long_word5").help("Set 5nd long word(32bit) of fhss channel mask").empty_values(false))
            .arg(Arg::with_name("long_word6").help("Set 6rd long word(32bit) of fhss channel mask").empty_values(false))
            .arg(Arg::with_name("long_word7").help("Set 7th long word(32bit) of fhss channel mask").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-fhss-timing-configure").about("Set fhss timing configure such as bc_dwell_interval bc_interval bc_dwell_interval")
            .arg(Arg::with_name("uc_dwell_interval").help("set fhss timing configure bc_dwell_interval").empty_values(false))
            .arg(Arg::with_name("broadcast_interval").help("set fhss timing configure bc_interval").empty_values(false))
            .arg(Arg::with_name("bc_dwell_interval").help("set fhss timing configure bc_dwell_interval").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-fhss-uc-function").about("set fhss unicast channel function")
            .arg(Arg::with_name("channel_function").help("set fhss unicast channel function").empty_values(false))
            .arg(Arg::with_name("fixed_channel").help("set fhss unicast channel function: fixed channel").empty_values(false))
            .arg(Arg::with_name("dwell_interval").help("set fhss unicast channel function: dewell interval").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-fhss-bc-function").about("set fhss broadcast channel function")
            .arg(Arg::with_name("channel_function").help("set fhss broadcast channel function").empty_values(false))
            .arg(Arg::with_name("fixed_channel").help("set fhss broadcast channel function: fixed channel").empty_values(false))
            .arg(Arg::with_name("dwell_interval").help("set fhss broadcast channel function: dewell interval").empty_values(false))
            .arg(Arg::with_name("broadcast_interval").help("set fhss broadcast channel function: broadcast interval").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-wisun-pan-id").about("Set wisun pan id")
            .arg(Arg::with_name("pan_id").help("Set wisun pan id").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-wisun-pan-size").about("Set wisun pan size. 1: small, 8: medium(100~800 device), 15: large(800~1500), 25: extreme large(<2500), 255: auto")
            .arg(Arg::with_name("pan_size").help("Set wisun pan size").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-wisun-gtk-key").about("Set wisun index gtk key. Usage: >wsbrd_cli set-wisun-gtk-key \"k0:k2:k3:...:k15\" ")
            .arg(Arg::with_name("gtk_key").help("Set wisun index gtk key").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-wisun-gtk-active-key").about("Set wisun gtk active key.")
            .arg(Arg::with_name("gtk_index").help("Set wisun gtk active key").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("set-wisun-key-lifetime").about("set wisun key lifetime, include GTK, PMK and PTK")
            .arg(Arg::with_name("gtk_lifetime").help("set wisun GTK lifetime").empty_values(false))
            .arg(Arg::with_name("pmk_lifetime").help("set wisun PMK lifetime").empty_values(false))
            .arg(Arg::with_name("ptk_lifetime").help("set wisun PTK lifetime").empty_values(false))
        ,)
        .subcommand(SubCommand::with_name("create-udp-socket").about("Create a UDP socket and indicate port number")
            .arg(Arg::with_name("udp_port").help("UDP port input as a 16bit usigned integer").empty_values(false))
        ,)
        .get_matches();
    let dbus_user = matches.is_present("user");

    match matches.subcommand_name() {
        Some("status")                      => do_status(dbus_user),
        Some("start-fan10")                 => do_startfan10(dbus_user),
        Some("stop-fan10")                  => do_stopfan10(dbus_user),
        Some("get-network-state")           => get_networkstate(dbus_user),
        Some("get-network-name")            => get_networkname(dbus_user),
        Some("get-wisun-phy-configs")       => get_wisun_phy_configs(dbus_user),
        Some("get-timing-parameters")       => get_timing_parameters(dbus_user),
        Some("get-fhss-channel-mask")       => get_fhss_channel_mask(dbus_user),
        Some("get-fhss-timing-configure")   => get_fhss_timing_configure(dbus_user),
        Some("get-wisun-pan-id")            => get_wisun_pan_id(dbus_user),
        Some("get-wisun-pan-size")          => get_wisun_pan_size(dbus_user),
        Some("get-wisun-gtk-keys")          => get_wisun_gtks(dbus_user),
        Some("get-wisun-gtk-active-key-index")  => get_wisun_gtk_active_key_index(dbus_user),
        Some("get-wisun-cfg-settings")      => get_wisun_cfg_settings(dbus_user),
        Some("set-network-name")            => {
            if let Some(subcmd) = matches.subcommand_matches("set-network-name") {
                if let Some(nwkname) = subcmd.value_of("nwk_name") {
                    wisun_nwkname = nwkname.to_string();
                }
            }
            set_networkname(dbus_user, wisun_nwkname)
        }
        Some("set-wisun-phy-configs")       => {
            if let Some(subcmd) = matches.subcommand_matches("set-wisun-phy-configs") {
                if let Some(tempval) = subcmd.value_of("domain") {
                    wisun_domain = tempval.parse::<u8>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("class") {
                    wisun_class = tempval.parse::<u8>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("mode") {
                    wisun_mode = tempval.parse::<u8>().unwrap();
                }
            }
            set_wisun_phy_configs(dbus_user, wisun_domain, wisun_class, wisun_mode)
        }
        Some("set-timing-parameters")       => {
            if let Some(subcmd) = matches.subcommand_matches("set-timing-parameters") {
                if let Some(tempval) = subcmd.value_of("trickle_imin") {
                    trickle_imin = tempval.parse::<u16>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("trickle_imax") {
                    trickle_imax = tempval.parse::<u16>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("trickle_k") {
                    trickle_k = tempval.parse::<u8>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("pan_timeout") {
                    pan_timeout = tempval.parse::<u16>().unwrap();
                }
            }
            set_timing_parameters(dbus_user, trickle_imin, trickle_imax, trickle_k, pan_timeout)
        }
        Some("set-fhss-channel-mask-f4b")       => {
            if let Some(subcmd) = matches.subcommand_matches("set-fhss-channel-mask-f4b") {
                if let Some(tempval) = subcmd.value_of("long_word0") {
                    long_word0 = tempval.parse::<u32>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("long_word1") {
                    long_word1 = tempval.parse::<u32>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("long_word2") {
                    long_word2 = tempval.parse::<u32>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("long_word3") {
                    long_word3 = tempval.parse::<u32>().unwrap();
                }
            }
            set_fhss_channel_mask_f4b(dbus_user, long_word0, long_word1, long_word2, long_word3)
        }
        Some("set-fhss-channel-mask-l4b")       => {
            if let Some(subcmd) = matches.subcommand_matches("set-fhss-channel-mask-l4b") {
                if let Some(tempval) = subcmd.value_of("long_word4") {
                    long_word4 = tempval.parse::<u32>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("long_word5") {
                    long_word5 = tempval.parse::<u32>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("long_word6") {
                    long_word6 = tempval.parse::<u32>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("long_word7") {
                    long_word7 = tempval.parse::<u32>().unwrap();
                }
            }
            set_fhss_channel_mask_l4b(dbus_user, long_word4, long_word5, long_word6, long_word7)
        }
        Some("set-fhss-timing-configure")       => {
            if let Some(subcmd) = matches.subcommand_matches("set-fhss-timing-configure") {
                if let Some(tempval) = subcmd.value_of("uc_dwell_interval") {
                    uc_dwell_interval = tempval.parse::<u8>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("broadcast_interval") {
                    broadcast_interval = tempval.parse::<u32>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("bc_dwell_interval") {
                    bc_dwell_interval = tempval.parse::<u8>().unwrap();
                }
            }
            set_fhss_timing_configure(dbus_user, uc_dwell_interval, broadcast_interval, bc_dwell_interval)
        }
        Some("set-fhss-uc-function")       => {
            if let Some(subcmd) = matches.subcommand_matches("set-fhss-uc-function") {
                if let Some(tempval) = subcmd.value_of("channel_function") {
                    channel_function = tempval.parse::<u8>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("fixed_channel") {
                    fixed_channel = tempval.parse::<u16>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("dwell_interval") {
                    dwell_interval = tempval.parse::<u8>().unwrap();
                }
            }
            set_fhss_uc_function(dbus_user, channel_function, fixed_channel, dwell_interval)
        }
        Some("set-fhss-bc-function")       => {
            if let Some(subcmd) = matches.subcommand_matches("set-fhss-bc-function") {
                if let Some(tempval) = subcmd.value_of("channel_function") {
                    channel_function = tempval.parse::<u8>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("fixed_channel") {
                    fixed_channel = tempval.parse::<u16>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("dwell_interval") {
                    dwell_interval = tempval.parse::<u8>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("broadcast_interval") {
                    broadcast_interval = tempval.parse::<u32>().unwrap();
                }
            }
            set_fhss_bc_function(dbus_user, channel_function, fixed_channel, dwell_interval, broadcast_interval)
        }
        Some("set-wisun-pan-id")       => {
            if let Some(subcmd) = matches.subcommand_matches("set-wisun-pan-id") {
                if let Some(tempval) = subcmd.value_of("pan_id") {
                    pan_id = tempval.parse::<u16>().unwrap();
                }
            }
            set_wisun_pan_id(dbus_user, pan_id)
        }
        Some("set-wisun-pan-size")       => {
            if let Some(subcmd) = matches.subcommand_matches("set-wisun-pan-size") {
                if let Some(tempval) = subcmd.value_of("pan_size") {
                    pan_size = tempval.parse::<u16>().unwrap();
                }
            }
            set_wisun_pan_size(dbus_user, pan_size)
        }
        Some("set-wisun-gtk-key")       => {
            if let Some(subcmd) = matches.subcommand_matches("set-wisun-gtk-key") {
                if let Some(tempval) = subcmd.value_of("gtk_key") {
                    gtk_key = tempval.to_string();
                }
            }
            set_wisun_gtk_key(dbus_user, gtk_key)
        }
        Some("set-wisun-gtk-active-key")       => {
            if let Some(subcmd) = matches.subcommand_matches("set-wisun-gtk-active-key") {
                if let Some(tempval) = subcmd.value_of("gtk_index") {
                    gtk_index = tempval.parse::<u8>().unwrap();
                }
            }
            set_wisun_gtk_active_key(dbus_user, gtk_index)
        }
        Some("set-wisun-key-lifetime")       => {
            if let Some(subcmd) = matches.subcommand_matches("set-wisun-key-lifetime") {
                if let Some(tempval) = subcmd.value_of("gtk_lifetime") {
                    gtk_lifetime = tempval.parse::<u32>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("pmk_lifetime") {
                    pmk_lifetime = tempval.parse::<u32>().unwrap();
                }
                if let Some(tempval) = subcmd.value_of("ptk_lifetime") {
                    ptk_lifetime = tempval.parse::<u32>().unwrap();
                }
            }
            set_wisun_key_lifetime(dbus_user, gtk_lifetime, pmk_lifetime, ptk_lifetime)
        }
        Some("create-udp-socket")       => {
            if let Some(subcmd) = matches.subcommand_matches("create-udp-socket") {
                if let Some(tempval) = subcmd.value_of("udp_port") {
                    udp_port = tempval.parse::<u16>().unwrap();
                }
            }
            create_udp_socket(dbus_user, udp_port)
        }
        _ => Ok(()), // Already covered by AppSettings::SubcommandRequired
    }

}
