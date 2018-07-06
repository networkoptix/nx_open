SELECT clouddb.account.email
	FROM clouddb.account
LEFT JOIN cloudportal.api_account 
	ON clouddb.account.email = cloudportal.api_account.email
WHERE cloudportal.api_account.email IS NULL AND clouddb.account.status_code<>4
