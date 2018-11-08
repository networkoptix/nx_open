SELECT clouddb.account.email
	FROM clouddb.account
RIGHT JOIN cloudportal.api_account 
	ON clouddb.account.email = cloudportal.api_account.email
WHERE clouddb.account.email IS NULL
