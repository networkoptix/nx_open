@echo off
python merge_dev.py -r dev_2.3.1_ui --preview
echo "Press Ctrl-C if you are not sure in merging"
pause
python merge_dev.py -r dev_2.3.1_ui