PROJ_DIR=`pwd`
cd /home/kutyacica/android/
TOP=`pwd`
export TOP
source build/envsetup.sh
lunch aosp_arm64-user
cd $PROJ_DIR
mm
cp $TOP/out/target/product/generic_arm64/system/bin/tee .
