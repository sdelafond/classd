/**
 * This is a sample designed to disable all of the scanning. 
 */
{
  "gateway" : "0.0.0.0",

  /* By default, spoof a host for 5 seconds after not seeing any traffic */
  "timeout" : 5.0,

  /* By default spoof a host every 2 seconds */
  "rate" : 2.0,

  /* Send broadcast spoof */
  "broadcast" : false,

  "networks" : [{
      "enabled" : false,
      "ip" : "0.0.0.0",
      "netmask" : "0.0.0.0",
      "spoof" : true,
      "passive" : true,
      "target" : "0.0.0.0"
  }],

  "mac_addresses" : [
      "00:11:22:33:44:55", "0:1:2:3:4:5"
  ]
}