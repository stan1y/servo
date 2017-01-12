create database servodb encoding 'UTF8';

\connect servodb;

create table session (
	client		varchar(36) primary key,
	expire_on	timestamp not null
);

create user servo with password 'test';
grant all privileges on table session to servo;