update item set str_val = $3, json_val = $4, blob_val = $5, last_write = now(),
	where client = $1 and key = $2