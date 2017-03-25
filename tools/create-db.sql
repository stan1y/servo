create database servodb encoding 'UTF8';

\connect servodb;

create table session (
	client		varchar(36) primary key,
	expire_on	timestamp not null
);

create table item (
	key			varchar(255) primary key,
	client		varchar(36) references session (client),
	str_val		text,
	json_val	json,
	blob_val	bytea
);

create user servo with password 'test';
grant all privileges on table session to servo;
grant all privileges on table item to servo;