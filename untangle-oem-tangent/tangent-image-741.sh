#!/bin/bash

if [ ! $# -eq 1 ] ; then
    echo "usage $0 <VOUCHER>"
    exit 1
fi

#
# kill firefox
#
echo "Closing firefox..."
killall firefox-bin &> /dev/null
killall /usr/lib/iceweasel/firefox-bin &> /dev/null

#
# create UID
#
echo "Creating UID..."
cp /usr/share/untangle/bin/utactivate /tmp/
sed -i 's/.*utregister.*//' /tmp/utactivate
/tmp/utactivate
rm -f /tmp/utactivate

echo "Downloading Package cache..."
sleep 5
apt-get update
if [ ! $? -eq 0 ] ; then
    echo "ERROR: Downloading packages failed. Are you online?"
    exit 1
fi

#
# register UID with store
#
MYUID="`cat /usr/share/untangle/popid | head -c 19`"
#VOUCHER="ALD1210-20100305A2GSF3OTP7F"
VOUCHER=$1
CUSTOMERID="5873"
URL="http://store.untangle.com/untangle_admin/oem/redeem-voucher.php?vc=$VOUCHER&uid=$MYUID&sid=$CUSTOMERID"

echo "Redeeming Voucher..."
sleep 5
OUTPUT="`curl $URL`"

if [ ! $? -eq 0 ] ; then
    echo "ERROR: $OUTPUT"
    echo "ERROR: is the voucher valid? Call Untangle"
    exit 1
fi
if [ ! "$OUTPUT" = "success" ] ; then
    echo "ERROR: $OUTPUT"
    echo "ERROR: is the voucher valid? Call Untangle"
    exit 1
fi

echo "Successfully redeemed Voucher"
sleep 5

#
# download apps
#
# Tangent libitem list:
#untangle-libitem-adblocker
#untangle-libitem-clam
#untangle-libitem-cpd
#untangle-libitem-firewall
#untangle-libitem-ips
#untangle-libitem-openvpn
#untangle-libitem-phish
#untangle-libitem-protofilter
#untangle-libitem-reporting
#untangle-libitem-shield
#untangle-libitem-spamassassin
#untangle-libitem-spyware
#untangle-libitem-adconnector
#untangle-libitem-boxbackup
#untangle-libitem-branding
#untangle-libitem-faild
#untangle-libitem-kav
#untangle-libitem-policy
#untangle-libitem-professional-package
#untangle-libitem-sitefilter
#untangle-libitem-splitd
#untangle-libitem-support

LIBITEMS="untangle-libitem-adblocker untangle-libitem-clam untangle-libitem-cpd untangle-libitem-firewall untangle-libitem-ips untangle-libitem-openvpn untangle-libitem-phish untangle-libitem-protofilter untangle-libitem-reporting untangle-libitem-shield untangle-libitem-spamassassin untangle-libitem-spyware untangle-libitem-adconnector untangle-libitem-boxbackup untangle-libitem-branding untangle-libitem-faild untangle-libitem-policy untangle-libitem-professional-package untangle-libitem-sitefilter untangle-libitem-support"
RACK_NODES="untangle-node-clam untangle-node-firewall untangle-node-ips untangle-node-phish untangle-node-protofilter untangle-node-spamassassin untangle-node-spyware untangle-node-kav untangle-node-sitefilter"
SERVICE_NODES="untangle-node-openvpn untangle-node-reporting untangle-node-adconnector untangle-node-boxbackup untangle-node-faild untangle-node-policy untangle-node-support untangle-node-shield"

echo "apt-get install --yes --force-yes $LIBITEMS"
apt-get install --yes --force-yes $LIBITEMS

if [ ! $? -eq 0 ] ; then
    echo "ERROR: Failed to download libitems successfully - call Untangle or email dmorris@untangle.com"
    exit 1
fi

#
# install oem package
#
echo "apt-get install --yes --force-yes untangle-oem-tangent"
apt-get install --yes --force-yes untangle-oem-tangent


# restart so untangle-vm sees new nodes (ucli register doesn't work pre-registration)
/etc/init.d/untangle-vm restart

#
# install apps in rack
#
for i in $RACK_NODES ; do 
    echo "Installing $i" 
    TID="`ucli -p 'Default Rack' instantiate $i`"
    if [ ! $? -eq 0 ] ; then
        echo "ERROR: Failed to install libitems successfully - call Untangle or email dmorris@untangle.com"
        exit 1
    fi    
    ucli start $TID &> /dev/null
    # ignore return code (some aren't supposed to start)
done

for i in $SERVICE_NODES ; do 
    echo "Installing $i" 
    TID="`ucli instantiate $i`"
    if [ ! $? -eq 0 ] ; then
        echo "ERROR: Failed to install libitems successfully - call Untangle or email dmorris@untangle.com"
        exit 1
    fi    
    ucli start $TID &> /dev/null
    # ignore return code (some aren't supposed to start)
done

#
# unset password
#
echo "Unsetting password..."
if ! sudo grep -qE '^root:(\*|YKN4WuGxhHpIw|$1$3kRMklXp$W/hDwKvL8GFi5Vdo3jtKC\.|CHANGEME):' /etc/shadow ; then
    echo "Resetting root password"
    perl -pe "s/^root:.*?:/root:CHANGEME:/" /etc/shadow > /tmp/newshadow
    cp -f /tmp/newshadow /etc/shadow
    rm -f /tmp/newshadow
fi

#
# power off
#
echo "Finished"