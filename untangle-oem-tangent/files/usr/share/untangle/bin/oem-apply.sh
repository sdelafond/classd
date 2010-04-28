#! /bin/sh

# apply files
rsync -Ha /usr/share/untangle-oem-*/ /

# rename grub titles
sed -i 's|^\(title.*\)Debian GNU/Linux, kernel|\1Kernel|' /boot/grub/menu.lst
sed -i 's|^\(title.*\)-untangle|\1|' /boot/grub/menu.lst

# change default settings
psql -U postgres uvm -c "update settings.u_mail_settings set from_address = 'webhawk@webhawk.untangle.com' where from_address = 'untangle@untangle.example.com'"
psql -U postgres uvm -c "update settings.u_address_settings set hostname = 'webhawk.example.com' where hostname = 'untangle.example.com'"

# set default hostname if still default
if [ "`cat /etc/hostname`" -eq "untangle.example.com" ] ; then
    echo "webhawk.example.com" > /etc/hostname
    hostname "webhawk.example.com"
fi
if [ ! -f /etc/hostname ] ; then
    echo "webhawk.example.com" > /etc/hostname
    hostname "webhawk.example.com"
fi

# set bootsplash
splashy_config -s tangent
update-initramfs -u

# replace license files
LIC_FILE=/usr/share/untangle/toolbox/untangle-node-license-impl/LicenseProfessional.txt
if [ -f $LIC_FILE ] ; then
    cat > $LIC_FILE <<EOF
Contact Tangent for more details.
EOF
fi
LIC_FILE=/usr/share/untangle/lib/untangle-libuvm-api/LicenseStandard.txt
if [ -f $LIC_FILE ] ; then
    cat > $LIC_FILE <<EOF
Contact Tangent for more details.
EOF
fi

exit 0