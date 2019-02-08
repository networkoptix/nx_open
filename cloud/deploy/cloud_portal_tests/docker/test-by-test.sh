#!/bin/bash  
grunt testportal:userclean1:default
kill $(lsof -t -i :4444)
grunt testportal:userclean2:default
kill $(lsof -t -i :4444)
grunt testportal:account1:default
kill $(lsof -t -i :4444)
grunt testportal:account2:default
kill $(lsof -t -i :4444)
grunt testportal:login1:default
kill $(lsof -t -i :4444)
grunt testportal:login2:default
kill $(lsof -t -i :4444)
grunt testportal:login3:default
kill $(lsof -t -i :4444)
grunt testportal:register1:default
kill $(lsof -t -i :4444)
grunt testportal:register2:default
kill $(lsof -t -i :4444)
grunt testportal:register3:default
kill $(lsof -t -i :4444)
grunt testportal:restorepass1:default
kill $(lsof -t -i :4444)
grunt testportal:restorepass2:default
kill $(lsof -t -i :4444)
grunt testportal:restorepass3:default
kill $(lsof -t -i :4444)
grunt testportal:syspage1:default
kill $(lsof -t -i :4444)
grunt testportal:syspage2:default
kill $(lsof -t -i :4444)
grunt testportal:syspage3:default
kill $(lsof -t -i :4444)
grunt testportal:systems:default
kill $(lsof -t -i :4444)
grunt testportal:smoke:default