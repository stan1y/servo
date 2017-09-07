create table item (
	key			varchar(255),
	client		varchar(36),
	last_read	timestamp not null,
	last_write	timestamp not null,
	str_val		text,
	json_val	json,
	blob_val	bytea,
	primary key(key, client)
);

create function servo_get_item(c varchar(36), k varchar(255))
	returns table(str_val text, json_val json, blob_val bytea) as $$
begin
	update item i set last_read = now() where i.key = k and i.client = c;
	return query select i.str_val, i.json_val, i.blob_val from item i where i.key = k and i.client = c;
end;
$$ language plpgsql;
