insert into item (client, key, last_read, last_write, str_val, json_val, blob_val)
	values (%(client)s, %(key)s, now(), now(), %(str_val)s, %(json_val)s, %(blob_val)s)
