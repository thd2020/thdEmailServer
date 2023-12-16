CREATE DATABASE IF NOT EXISTS smtp_server;
USE smtp_server;
CREATE TABLE IF NOT EXISTS `users` (
    `user_id` INT NOT NULL AUTO_INCREMENT,
    `username` VARCHAR(50) NOT NULL,
    `password` VARCHAR(50) NOT NULL,
    `path` VARCHAR(50) NOT NULL COMMENT 'user mail storage path',
    PRIMARY KEY (`user_id`)
);
CREATE TABLE IF NOT EXISTS `sent_mails` (
    `sm_id` INT AUTO_INCREMENT,
    `user_id` INT NOT NULL,
    `time` DATETIME,
    `title` VARCHAR(30),
    `rcpt` VARCHAR(30) COMMENT 'recipient',
    `data_path` VARCHAR(150),
    `size` INT 0 COMMENT 'size in bytes',
    `processed` INT 0,
    PRIMARY KEY (`sm_id`),
    FOREIGN KEY (`user_id`) REFERENCES `users`(`user_id`)
);
CREATE TABLE IF NOT EXISTS `received_mails` (
    `rm_id` INT AUTO_INCREMENT,
    `user_id` INT NOT NULL,
    `time` DATETIME,
    `title` VARCHAR(30),
    `from` VARCHAR(30),
    `data_path` VARCHAR(150),
    `size` INT 0 COMMENT 'size in bytes',
    `processed` INT 0,
    PRIMARY KEY (`rm_id`),
    FOREIGN KEY (`user_id`) REFERENCES `users`(`user_id`)
);