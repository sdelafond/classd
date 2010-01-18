#!/usr/bin/env ruby
## Copyright (c) 2003-2008 Untangle, Inc.
##  All rights reserved.
## 
##  This software is the confidential and proprietary information of
##  Untangle, Inc. ("Confidential Information"). You shall
##  not disclose such Confidential Information.
## 
##  $Id: ADConnectorImpl.java 15443 2008-03-24 22:53:16Z amread $
## 

require "dbi"
require "json"
require "cgi"
require "ftools"
require "logger"

SingleNicFlag="/usr/share/untangle-arp-eater/flag"

CreatePopId="/usr/share/untangle/bin/createpopid.rb"
Activate="/usr/share/untangle/bin/utactivate"
PopId="/usr/share/untangle/popid"

SetTimezone="/usr/share/untangle/bin/uttimezone"


RegistrationInfo="/usr/share/untangle/registration.info"
RegistrationDone="/usr/share/untangle/.regdone"

$logger = Logger.new( STDOUT )

def usage()
  puts "USAGE #{ARGV[0]} <config-file>"
  exit 254
end

## Utility from the alpaca to run a command with a timeout
def run_command( command, timeout = 30 )
  p = nil
  begin
    status = 127
    t = Thread.new do 
      p = IO.popen( command )
      $logger.info( "running the command: #{command}" )
      p.each_line { |line| $logger.info( line.strip ) }
      pid, status = Process.wait2( p.pid )
      status = status.exitstatus
    end
    
    ## Kill the thread
    t.join( timeout )
    t.kill if t.alive?
    
    return status
  ensure
    p.close unless p.nil?
  end
end

## All of the pacakges should already be inside of the cache.
INST_OPTS = " -o DPkg::Options::=--force-confnew --yes --force-yes --fix-broken --purge --no-download "

def install_packages( config )
  packages = config["packages"]

  if ( packages.nil? || !( packages.is_a? Array ) || packages.empty? )
    $logger.info( "No packages to install." )
    return
  end

  run_command( "apt-get install #{INST_OPTS} #{packages.join( " " )}", 30 * 60 )
  
  run_command( "echo '' > /etc/apt/sources.list" )
end

## Modify this function with extreme caution, escaping  binary characters has caused numerous
## delays and random password errors
def set_password_hash( config, dbh )
  password_hash = config["password"]
  if ( password_hash.nil? || /^[0-9a-fA-F]{48}$/.match( password_hash ).nil? )
    $logger.warn( "Invalid password hash: #{password_hash}" )
    return
  end

  database_string = ""
  ( password_hash.length / 2 ).times do |n|
    database_string << '\\\\%03o' % password_hash[2*n .. ( 2*n ) + 1].to_i( 16 )
  end

  admin_settings_id = dbh.select_one(<<SQL).first
SELECT nextval('hibernate_sequence');
SQL

  dbh.do("DELETE FROM u_user")
  dbh.do("DELETE FROM u_admin_settings")

  dbh.do("INSERT INTO u_admin_settings (admin_settings_id) VALUES (#{admin_settings_id})")  
  
  dbh.do("INSERT INTO u_user ( id, login, password, email, name, write_access, reports_access, notes, send_alerts, admin_setting_id ) VALUES ( nextval('hibernate_sequence'),'admin','#{database_string}','[no email]', 'System Administrator',true, true, '[no description]',false, ? )", admin_settings_id )
end

def setup_registration( config, dbh )
  unless File.exists?( CreatePopId )
    $logger.warn( "Unable to create pop id, missing the script #{CreatePopId}" )
    return
  end

  load( CreatePopId, true )
  
  unless File.exists?( PopId )
    $logger.warn( "POPID file doesn't exists: #{PopId}" )
    return
  end
  
  registration = config["registration"]

  popid = ""

  ## Popid is just the first line.
  File.open( PopId, "r" ) { |f| f.each_line { |l| popid = l ; break }}

  popid.strip!

  ## This will get updated later automatically.
  if popid.empty?
    $logger.warn( "POPID is empty" )
    return
  end
    
  if registration.nil?
    $logger.warn( "WARNING : Missing registration information, assuming bogus values." )
    registration={}
    registration["email"] = "unset@example.com"
    registration["name"] = "unset"
    registration["numseats"] = 5
  end

  url_string = []
  
  registration["regKey"] = popid

  name = registration["name"]
  unless name.nil?
    name = name.split( " " )
    
    case name.length
    when 0
      ## Nothing to do
    when 1
      registration["firstName"] = name[0]
    else
      registration["lastName"] = name.pop
      registration["firstName"] = name.join( " " )
    end
  end

  registration["version"] = `cat /usr/share/untangle/lib/untangle-libuvm-api/PUBVERSION`
  
  [ "regKey", "emailAddr", "name", "firstName", "lastName", "numSeats", "find_untangle", "country", "environment", "version", "brand" ].each do |key|
    param = registration[key]

    next if param.nil?
    param = param.to_s
    param.strip!
    next if param.empty?

    url_string << "#{CGI.escape( key )}=#{CGI.escape( param.to_s )}"
  end
    
  File.rm_f( RegistrationInfo )
  File.rm_f( RegistrationDone )
  File.open( RegistrationInfo, "w" ) { |f| f.puts url_string.join( "&" ) }

  unless File.exists?( Activate )
    $logger.warn( "Unable to activate, missing the script #{Activate}" )
    return
  end
  
  run_command( Activate, 60 )
end

def write_brand(config)
  registration = config["registration"]
  return if registration.nil?
  brand_string = registration["brand"]
  return if brand_string.nil?

  File.open( "/usr/share/untangle/tmp/brand", "w" ) do |f|
    f.puts( brand_string )
  end
end

def setup_alpaca( config_file )
  ## Initialize the settings for the alpaca.
  begin
    Kernel.system(<<EOF)

/etc/init.d/untangle-net-alpaca stop

sleep 2

BASE_DIR="/var/lib/rails/untangle-net-alpaca"
MONGREL_ENV="production"
[ -f /etc/default/untangle-net-alpaca ] && . /etc/default/untangle-net-alpaca
cd "${BASE_DIR}" || {
  echo "[`date`] Unable to change into ${BASE_DIR}"
  exit -1
}

rake --trace -s alpaca:preconfigure RAILS_ENV=${MONGREL_ENV} CONFIG_FILE="#{config_file}" || { 
  echo "[`date`] could not load preconfiguration settings."
  exit -2
}

## This hack allows the user to save network settings
## without modification to the internal interface which has no settings.
## This is done here so we do not have to modify the alpaca.
echo "Changing the internal nointerface to bridge."
echo "UPDATE interfaces SET config_type='bridge' WHERE name='Internal';" | sqlite3 "${BASE_DIR}/database/${MONGREL_ENV}.db"
EOF
  ensure
    Kernel.system(<<EOF)
/etc/init.d/untangle-net-alpaca restart
EOF
  end
end

def set_timezone( config )
  unless File.exists?( SetTimezone )
    $logger.warn( "Unable to set the timezone, missing the script #{SetTimezone}" )
    return
  end

  current_timezone = config["timezone"]
  if ( current_timezone.nil? || !File.exists?( "/usr/share/zoneinfo/#{current_timezone}" ))
    current_timezone = "GMT"
  end

  ## Initialize the timezone using uttimezone
  Kernel.system( "#{SetTimezone} #{current_timezone}" )
end

usage if ARGV.length != 1

config_file = ARGV[0]

config = ""

unless File.exist?( config_file )
  $logger.info( "The config_file: #{config_file} doesn't exists." )
  exit 0
end

$logger.info( "Using the config file" )
Kernel.system( "cat #{config_file}" )

$logger.info( "Converting the file to UTF-8" )
config_file_utf8=config_file.sub( /.js$/, "-utf8.js" )
config_file_utf8="#{config_file}-utf8" if ( config_file_utf8 == config_file )
Kernel.system( "/usr/bin/iconv -f ISO-8859-1 -t UTF-8 #{config_file} > #{config_file_utf8} || /usr/bin/iconv -f MS936 -t UTF-8 #{config_file} > #{config_file_utf8}" )
Kernel.system( "mv #{config_file_utf8} #{config_file}" )


File.open( config_file, "r" ) { |f| f.each_line { |l| config << l }}
config = ::JSON.parse( config )

## Disable the setup wizard by removing the router settings and clearing the database
Kernel.system( <<EOF )
echo "[`date`] Recreating the database for Single NIC Mode."
[ -f /etc/default/untangle-vm ] && . /etc/default/untangle-vm
dropdb -U postgres uvm
createuser -U postgres -dSR untangle 2>/dev/null
createdb -O postgres -U postgres uvm
/usr/share/untangle/bin/update-schema settings uvm
/usr/share/untangle/bin/update-schema events uvm
/usr/share/untangle/bin/update-schema settings untangle-node-router
/usr/share/untangle/bin/update-schema events untangle-node-router

echo "[`date`] Regenerating the SSH keys."
rm -f /etc/ssh/ssh_host*
dpkg-reconfigure openssh-server
EOF

## Insert the password for the user
dbh = DBI.connect('DBI:Pg:uvm', 'postgres')

install_packages( config )

set_password_hash( config, dbh )

setup_alpaca( config_file )

## Register after configuring the alpaca so the network is up.
setup_registration( config, dbh )

set_timezone( config )

write_brand(config)
