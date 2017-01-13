create database servodb encoding 'UTF8';

\connect servodb;

create table session (
	client		varchar(36) primary key,
	expire_on	timestamp not null
);

create table item (
	client		varchar(36) references session (client),
	key			varchar(255),
	str_val		varchar(1024),
	json_val	json,
	blob_val	bytea
);

create user servo with password 'test';
grant all privileges on table session to servo;
grant all privileges on table item to servo;