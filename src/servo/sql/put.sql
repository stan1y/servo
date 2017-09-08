update item set str_val = %(str_val)s, json_val = %(json_val)s, blob_val = %(blob_val)s, last_write = now()
	where client = %(client)s and key = %(key)s
