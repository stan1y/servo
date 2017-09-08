insert into item (client, key, last_read, last_write, str_val, json_val, blob_val)
	values (%s, %s, now(), now(), %s, %s, %s)
