insert into item (client, key, last_read, last_write, str_val, json_val, blob_val)
	values ($1, $2, now(), now(), $3, $4, $5)