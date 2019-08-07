# Enable coherency
# This sets AxPROT to 0b010, which should be correct for Linux
sudo /home/ubuntu/ultrazynqbuilder/utils/poke 0x80010000 0x02

# Copy the R5 firmware the the correct directory and launch it
echo "stop" | sudo tee /sys/class/remoteproc/remoteproc0/state
sudo cp /home/ubuntu/r5_runtime.elf /lib/firmware
echo "r5_runtime.elf" | sudo tee /sys/class/remoteproc/remoteproc0/firmware
echo "start" | sudo tee /sys/class/remoteproc/remoteproc0/state

