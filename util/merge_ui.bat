@echo off
python merge_dev.py -t prod_2.3.2 -r dev_2.3.2_gui -p
echo "Press Ctrl-C if you are not sure in merging"
pause
python merge_dev.py -t prod_2.3.2 -r dev_2.3.2_gui
