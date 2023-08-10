
DROP TABLE `authorization`;
DROP TABLE `session`;

CREATE OR REPLACE TABLE `session` (
  `id` INT UNSIGNED PRIMARY KEY NOT NULL AUTO_INCREMENT,
  `token` VARCHAR(64) UNIQUE NOT NULL,
  `created` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `last_update` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `valid_for` INT UNSIGNED NULL,
  `user` VARCHAR(32) NOT NULL,
  `user_agent` TEXT
);

CREATE OR REPLACE TABLE `authorization` (
  `id` INT UNSIGNED PRIMARY KEY NOT NULL AUTO_INCREMENT,
  `origin` VARCHAR(128) NOT NULL,
  `token` VARCHAR(64) NOT NULL,
  `session` INT UNSIGNED NOT NULL,
  `created` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `last_update` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `valid_for` INT UNSIGNED NULL,
  FOREIGN KEY (`session`) REFERENCES `session`(`id`) ON DELETE CASCADE,
  UNIQUE(`origin`,`token`),
  UNIQUE(`origin`,`session`)
);

delimiter //
CREATE OR REPLACE TRIGGER `session_update_last_update` BEFORE UPDATE ON `session`
FOR EACH ROW
BEGIN
  SET NEW.`last_update` = CURRENT_TIMESTAMP;
END;//
CREATE OR REPLACE TRIGGER `authorization_update_last_update` BEFORE UPDATE ON `authorization`
FOR EACH ROW
BEGIN
  SET NEW.`last_update` = CURRENT_TIMESTAMP;
END;//
delimiter ;

CREATE OR REPLACE VIEW `valid_session` AS
  SELECT * FROM `session`
   WHERE `created` <= CURRENT_TIMESTAMP
     AND `last_update` + INTERVAL 1 WEEK > CURRENT_TIMESTAMP
     AND (`valid_for` IS NULL OR `created` + INTERVAL `valid_for` SECOND > CURRENT_TIMESTAMP)
;

CREATE OR REPLACE VIEW `valid_authorization` AS
  SELECT
    `t`.*,
    `s`.`token` AS `session_token`,
    `t`.`last_update` + INTERVAL 10 MINUTE <= CURRENT_TIMESTAMP AS `stale`
  FROM `authorization` AS `t`
  INNER JOIN `valid_session` AS `s` ON `s`.`id` = `t`.`session`
  WHERE  `t`.`created` <= CURRENT_TIMESTAMP
    AND  `t`.`last_update` + INTERVAL 1 HOUR > CURRENT_TIMESTAMP
    AND (`t`.`valid_for` IS NULL OR `t`.`created` + INTERVAL `t`.`valid_for` SECOND > CURRENT_TIMESTAMP)
;

delimiter //
CREATE OR REPLACE PROCEDURE `auto_cleanup`()
BEGIN
  DELETE `session` FROM `session`
              LEFT JOIN `valid_session`
                     ON `valid_session`.`id`=`session`.`id`
                  WHERE `valid_session`.`id` IS NULL;
  DELETE `authorization` FROM `authorization`
                    LEFT JOIN `valid_authorization`
                           ON `valid_authorization`.`id`=`authorization`.`id`
                        WHERE `valid_authorization`.`id` IS NULL;
END;//
delimiter ;

CREATE OR REPLACE EVENT `e_auto_cleanup` ON SCHEDULE EVERY 1 HOUR DO CALL `auto_cleanup`;
