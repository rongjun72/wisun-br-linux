// This code was autogenerated with `dbus-codegen-rust -g -m None -d com.silabs.Wisun.BorderRouter -p /com/silabs/Wisun/BorderRouter`, see https://github.com/diwic/dbus-rs
use dbus as dbus;
#[allow(unused_imports)]
use dbus::arg;
use dbus::blocking;

pub trait OrgFreedesktopDBusPeer {
    fn ping(&self) -> Result<(), dbus::Error>;
    fn get_machine_id(&self) -> Result<String, dbus::Error>;
}

impl<'a, T: blocking::BlockingSender, C: ::std::ops::Deref<Target=T>> OrgFreedesktopDBusPeer for blocking::Proxy<'a, C> {

    fn ping(&self) -> Result<(), dbus::Error> {
        self.method_call("org.freedesktop.DBus.Peer", "Ping", ())
    }

    fn get_machine_id(&self) -> Result<String, dbus::Error> {
        self.method_call("org.freedesktop.DBus.Peer", "GetMachineId", ())
            .and_then(|r: (String, )| Ok(r.0, ))
    }
}

pub trait OrgFreedesktopDBusIntrospectable {
    fn introspect(&self) -> Result<String, dbus::Error>;
}

impl<'a, T: blocking::BlockingSender, C: ::std::ops::Deref<Target=T>> OrgFreedesktopDBusIntrospectable for blocking::Proxy<'a, C> {

    fn introspect(&self) -> Result<String, dbus::Error> {
        self.method_call("org.freedesktop.DBus.Introspectable", "Introspect", ())
            .and_then(|r: (String, )| Ok(r.0, ))
    }
}

pub trait OrgFreedesktopDBusProperties {
    fn get<R0: for<'b> arg::Get<'b> + 'static>(&self, interface_name: &str, property_name: &str) -> Result<R0, dbus::Error>;
    fn get_all(&self, interface_name: &str) -> Result<arg::PropMap, dbus::Error>;
    fn set<I2: arg::Arg + arg::Append>(&self, interface_name: &str, property_name: &str, value: I2) -> Result<(), dbus::Error>;
}

#[derive(Debug)]
pub struct OrgFreedesktopDBusPropertiesPropertiesChanged {
    pub interface_name: String,
    pub changed_properties: arg::PropMap,
    pub invalidated_properties: Vec<String>,
}

impl arg::AppendAll for OrgFreedesktopDBusPropertiesPropertiesChanged {
    fn append(&self, i: &mut arg::IterAppend) {
        arg::RefArg::append(&self.interface_name, i);
        arg::RefArg::append(&self.changed_properties, i);
        arg::RefArg::append(&self.invalidated_properties, i);
    }
}

impl arg::ReadAll for OrgFreedesktopDBusPropertiesPropertiesChanged {
    fn read(i: &mut arg::Iter) -> Result<Self, arg::TypeMismatchError> {
        Ok(OrgFreedesktopDBusPropertiesPropertiesChanged {
            interface_name: i.read()?,
            changed_properties: i.read()?,
            invalidated_properties: i.read()?,
        })
    }
}

impl dbus::message::SignalArgs for OrgFreedesktopDBusPropertiesPropertiesChanged {
    const NAME: &'static str = "PropertiesChanged";
    const INTERFACE: &'static str = "org.freedesktop.DBus.Properties";
}

impl<'a, T: blocking::BlockingSender, C: ::std::ops::Deref<Target=T>> OrgFreedesktopDBusProperties for blocking::Proxy<'a, C> {

    fn get<R0: for<'b> arg::Get<'b> + 'static>(&self, interface_name: &str, property_name: &str) -> Result<R0, dbus::Error> {
        self.method_call("org.freedesktop.DBus.Properties", "Get", (interface_name, property_name, ))
            .and_then(|r: (arg::Variant<R0>, )| Ok((r.0).0, ))
    }

    fn get_all(&self, interface_name: &str) -> Result<arg::PropMap, dbus::Error> {
        self.method_call("org.freedesktop.DBus.Properties", "GetAll", (interface_name, ))
            .and_then(|r: (arg::PropMap, )| Ok(r.0, ))
    }

    fn set<I2: arg::Arg + arg::Append>(&self, interface_name: &str, property_name: &str, value: I2) -> Result<(), dbus::Error> {
        self.method_call("org.freedesktop.DBus.Properties", "Set", (interface_name, property_name, arg::Variant(value), ))
    }
}

pub trait ComSilabsWisunBorderRouter {
    fn join_multicast_group(&self, arg0: Vec<u8>) -> Result<(), dbus::Error>;
    fn leave_multicast_group(&self, arg0: Vec<u8>) -> Result<(), dbus::Error>;
    fn set_mode_switch(&self, arg0: Vec<u8>, arg1: i32) -> Result<(), dbus::Error>;
    fn start_fan10(&self, arg0: Vec<u8>, arg1: i32) -> Result<(), dbus::Error>;
    fn stop_fan10(&self, arg0: Vec<u8>, arg1: i32) -> Result<(), dbus::Error>;
    fn set_slot_algorithm(&self, arg0: u8) -> Result<(), dbus::Error>;
    fn revoke_pairwise_keys(&self, arg0: Vec<u8>) -> Result<(), dbus::Error>;
    fn revoke_group_keys(&self, arg0: Vec<u8>, arg1: Vec<u8>) -> Result<(), dbus::Error>;
    fn gtks(&self) -> Result<Vec<Vec<u8>>, dbus::Error>;
    fn gaks(&self) -> Result<Vec<Vec<u8>>, dbus::Error>;
    fn lgtks(&self) -> Result<Vec<Vec<u8>>, dbus::Error>;
    fn lgaks(&self) -> Result<Vec<Vec<u8>>, dbus::Error>;
    fn nodes(&self) -> Result<Vec<(Vec<u8>, arg::PropMap)>, dbus::Error>;
    fn hw_address(&self) -> Result<Vec<u8>, dbus::Error>;
    fn wisun_network_name(&self) -> Result<String, dbus::Error>;
    fn wisun_size(&self) -> Result<String, dbus::Error>;
    fn wisun_domain(&self) -> Result<String, dbus::Error>;
    fn wisun_mode(&self) -> Result<u32, dbus::Error>;
    fn wisun_class(&self) -> Result<u32, dbus::Error>;
    fn wisun_phy_mode_id(&self) -> Result<u32, dbus::Error>;
    fn wisun_chan_plan_id(&self) -> Result<u32, dbus::Error>;
    fn wisun_pan_id(&self) -> Result<u16, dbus::Error>;
    fn wisun_fan_version(&self) -> Result<u8, dbus::Error>;
    /*********** new command */
    fn get_network_state(&self) -> Result<Vec<Vec<u8>>, dbus::Error>;
    fn get_timing_parameters(&self) -> Result<Vec<u16>, dbus::Error>;
    fn get_fhss_channel_mask(&self) -> Result<Vec<u32>, dbus::Error>;
    fn get_fhss_timing_configure(&self) -> Result<Vec<u32>, dbus::Error>;
    fn get_gtk_active_key_index(&self) -> Result<u8, dbus::Error>;
    fn get_wisun_cfg_settings(&self) -> Result<Vec<u16>, dbus::Error>;
    fn set_network_name(&self, arg0: String) -> Result<(), dbus::Error>;
    fn set_wisun_phy_configs(&self, arg0: u8, arg1: u8, arg2: u8) -> Result<(), dbus::Error>;
    fn set_timing_parameters(&self, arg0: u16, arg1: u16, arg2: u8, arg3: u16) -> Result<(), dbus::Error>;
    fn set_fhss_channel_mask_f4b(&self, arg0: u32, arg1: u32, arg2: u32, arg3: u32) -> Result<(), dbus::Error>;
    fn set_fhss_channel_mask_l4b(&self, arg0: u32, arg1: u32, arg2: u32, arg3: u32) -> Result<(), dbus::Error>;
    fn set_fhss_timing_configure(&self, arg0: u8, arg1: u32, arg2: u8) -> Result<(), dbus::Error>;
    fn set_fhss_uc_function(&self, arg0: u8, arg1: u16, arg2: u8) -> Result<(), dbus::Error>;
    fn set_fhss_bc_function(&self, arg0: u8, arg1: u16, arg2: u8, arg3: u32) -> Result<(), dbus::Error>;
    fn set_wisun_pan_id(&self, arg0: u16) -> Result<(), dbus::Error>;
    fn set_wisun_pan_size(&self, arg0: u16) -> Result<(), dbus::Error>;
    fn set_wisun_gtk_key(&self, arg0: Vec<u8>) -> Result<(), dbus::Error>;
    fn set_wisun_gtk_active_key(&self, arg0: u8) -> Result<(), dbus::Error>;
    fn set_wisun_key_lifetime(&self, arg0: u32, arg1: u32, arg2: u32) -> Result<(), dbus::Error>;
    fn create_udp_socket(&self, arg0: u16) -> Result<(), dbus::Error>;
    fn set_udp_dst_port(&self, arg0: u16) -> Result<(), dbus::Error>;
    fn socket_udp_sent_to(&self, arg0: Vec<u8>) -> Result<(), dbus::Error>;
    fn set_multicast_addr(&self, arg0: Vec<u8>) -> Result<(), dbus::Error>;
    fn set_udp_body_uint_repeat_time(&self, arg0: u8) -> Result<(), dbus::Error>;
    fn set_udp_tail(&self, arg0: Vec<u8>) -> Result<(), dbus::Error>;
}

impl<'a, T: blocking::BlockingSender, C: ::std::ops::Deref<Target=T>> ComSilabsWisunBorderRouter for blocking::Proxy<'a, C> {

    fn join_multicast_group(&self, arg0: Vec<u8>) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "JoinMulticastGroup", (arg0, ))
    }

    fn leave_multicast_group(&self, arg0: Vec<u8>) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "LeaveMulticastGroup", (arg0, ))
    }

    fn set_mode_switch(&self, arg0: Vec<u8>, arg1: i32) -> Result<(), dbus::Error> {
        println!("set_mode_switch: {}", arg1);
        self.method_call("com.silabs.Wisun.BorderRouter", "SetModeSwitch", (arg0, arg1, ))
    }

    fn start_fan10(&self, arg0: Vec<u8>, arg1: i32) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "startFan10", (arg0, arg1, ))
    }

    fn stop_fan10(&self, arg0: Vec<u8>, arg1: i32) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "stopFan10", (arg0, arg1, ))
    }

    fn set_slot_algorithm(&self, arg0: u8) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "SetSlotAlgorithm", (arg0, ))
    }

    fn revoke_pairwise_keys(&self, arg0: Vec<u8>) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "RevokePairwiseKeys", (arg0, ))
    }

    fn revoke_group_keys(&self, arg0: Vec<u8>, arg1: Vec<u8>) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "RevokeGroupKeys", (arg0, arg1, ))
    }

    fn gtks(&self) -> Result<Vec<Vec<u8>>, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "Gtks")
    }

    fn gaks(&self) -> Result<Vec<Vec<u8>>, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "Gaks")
    }

    fn lgtks(&self) -> Result<Vec<Vec<u8>>, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "Lgtks")
    }

    fn lgaks(&self) -> Result<Vec<Vec<u8>>, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "Lgaks")
    }

    fn nodes(&self) -> Result<Vec<(Vec<u8>, arg::PropMap)>, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "Nodes")
    }

    fn hw_address(&self) -> Result<Vec<u8>, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "HwAddress")
    }

    fn wisun_network_name(&self) -> Result<String, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "WisunNetworkName")
    }

    fn wisun_size(&self) -> Result<String, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "WisunSize")
    }

    fn wisun_domain(&self) -> Result<String, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "WisunDomain")
    }

    fn wisun_mode(&self) -> Result<u32, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "WisunMode")
    }

    fn wisun_class(&self) -> Result<u32, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "WisunClass")
    }

    fn wisun_phy_mode_id(&self) -> Result<u32, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "WisunPhyModeId")
    }

    fn wisun_chan_plan_id(&self) -> Result<u32, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "WisunChanPlanId")
    }

    fn wisun_pan_id(&self) -> Result<u16, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "WisunPanId")
    }

    fn wisun_fan_version(&self) -> Result<u8, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "WisunFanVersion")
    }

    /******** new command */
    fn get_network_state(&self) -> Result<Vec<Vec<u8>>, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "NetworkState")
    }

    fn set_network_name(&self, arg0: String) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "SetNetworkName", (arg0, ))
    }

    fn set_wisun_phy_configs(&self, arg0: u8, arg1: u8, arg2: u8) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "SetPhyConfigs", (arg0, arg1, arg2, ))
    }

    fn get_timing_parameters(&self) -> Result<Vec<u16>, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "GetTimingParam")
    }

    fn set_timing_parameters(&self, arg0: u16, arg1: u16, arg2: u8, arg3: u16) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "SetTimingParams", (arg0, arg1, arg2, arg3, ))
    }

    fn get_fhss_channel_mask(&self) -> Result<Vec<u32>, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "getFhssChannelMask")
    }

    fn get_fhss_timing_configure(&self) -> Result<Vec<u32>, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "getFhssTimingConfigure")
    }

    fn get_gtk_active_key_index(&self) -> Result<u8, dbus::Error> {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "getGtkActiveKeyIndex")
    }

    fn get_wisun_cfg_settings(&self) -> Result<Vec<u16>, dbus::Error>
    {
        <Self as blocking::stdintf::org_freedesktop_dbus::Properties>::get(&self, "com.silabs.Wisun.BorderRouter", "getWisunCfgSettings")
    }

    fn set_fhss_channel_mask_f4b(&self, arg0: u32, arg1: u32, arg2: u32, arg3: u32) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setFhssChMaskF4b", (arg0, arg1, arg2, arg3, ))
    }

    fn set_fhss_channel_mask_l4b(&self, arg0: u32, arg1: u32, arg2: u32, arg3: u32) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setFhssChMaskL4b", (arg0, arg1, arg2, arg3, ))
    }

    fn set_fhss_timing_configure(&self, arg0: u8, arg1: u32, arg2: u8) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setFhssTimingConfig", (arg0, arg1, arg2, ))
    }

    fn set_fhss_uc_function(&self, arg0: u8, arg1: u16, arg2: u8) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setFhssUCFuntion", (arg0, arg1, arg2, ))
    }

    fn set_fhss_bc_function(&self, arg0: u8, arg1: u16, arg2: u8, arg3: u32) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setFhssBCFuntion", (arg0, arg1, arg2, arg3, ))
    }

    fn set_wisun_pan_id(&self, arg0: u16) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setWisunPanId", (arg0, ))
    }

    fn set_wisun_pan_size(&self, arg0: u16) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setWisunPanSize", (arg0, ))
    }

    fn set_wisun_gtk_key(&self, arg0: Vec<u8>) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setWisunGtkKey", (arg0, ))
    }

    fn set_wisun_gtk_active_key(&self, arg0: u8) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setWisunGtkActiveKey", (arg0, ))
    }

    fn set_wisun_key_lifetime(&self, arg0: u32, arg1: u32, arg2: u32) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setWisunKeyLifetime", (arg0, arg1, arg2, ))
    }

    fn create_udp_socket(&self, arg0: u16) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "createUdpSocket", (arg0, ))
    }

    fn set_udp_dst_port(&self, arg0: u16) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setUdpDstPort", (arg0, ))
    }

    fn socket_udp_sent_to(&self, arg0: Vec<u8>) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "socketUdpSendTo", (arg0, ))
    }

    fn set_multicast_addr(&self, arg0: Vec<u8>) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setMultcastAddr", (arg0, ))
    }

    fn set_udp_body_uint_repeat_time(&self, arg0: u8) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setUdpBodyUintRepeatTtime", (arg0, ))
    }

    fn set_udp_tail(&self, arg0: Vec<u8>) -> Result<(), dbus::Error> {
        self.method_call("com.silabs.Wisun.BorderRouter", "setUdpTail", (arg0, ))
    }

}
