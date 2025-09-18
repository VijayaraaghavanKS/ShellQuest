# Kickstart for ShellQuest Fedora 42 Live ISO
url --url=https://dl.fedoraproject.org/pub/fedora/linux/releases/42/Server/x86_64/os/
lang en_US.UTF-8
keyboard us
timezone Asia/Kolkata
rootpw --plaintext rootpass
user --name=learner --plaintext --password=learn --shell=/usr/local/bin/shellquest_gui

%packages
@core
kernel-devel
readline
gtk3
vte291
xfce4-terminal
@standard
@base-x
@xfce-desktop
%end

%post
# Install ShellQuest and GUI
cp /files/shellquest /usr/local/bin/
cp /files/shellquest_gui /usr/local/bin/
chmod +x /usr/local/bin/shellquest /usr/local/bin/shellquest_gui
echo "/usr/local/bin/shellquest" >> /etc/shells
echo "/usr/local/bin/shellquest_gui" >> /etc/shells

# Kernel module
mkdir /root/shellquest-module
cp /files/shellquest_stats.ko /root/shellquest-module/

# Sudoers
echo "learner ALL=(ALL) NOPASSWD: /usr/sbin/insmod,/usr/sbin/rmmod,/usr/sbin/chsh" > /etc/sudoers.d/shellquest
chmod 0440 /etc/sudoers.d/shellquest

# Autostart GUI
mkdir -p /home/learner/.config/autostart
cat << EOF > /home/learner/.config/autostart/shellquest.desktop
[Desktop Entry]
Type=Application
Exec=/usr/local/bin/shellquest_gui
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
Name=ShellQuest
EOF
chown learner:learner /home/learner/.config -R

# GRUB for modes
cat << EOF >> /etc/grub.d/40_custom
menuentry "Learning Mode" {
    set root=(cd0)
    linux /vmlinuz root=live:CDLABEL=Fedora-Server iso-scan/filename=/shellquest.iso learning=yes ---
    initrd /initrd.img
}
menuentry "Standard Mode" {
    set root=(cd0)
    linux /vmlinuz root=live:CDLABEL=Fedora-Server iso-scan/filename=/shellquest.iso ---
    initrd /initrd.img
}
EOF
grub2-mkconfig -o /boot/grub2/grub.cfg
%end
