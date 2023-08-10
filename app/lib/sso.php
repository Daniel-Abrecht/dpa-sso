<?php

class sso {
  public static function load(string $type, ...$id){
    switch($type){
      case 'Session':
      case 'sso\Session':
      case '\sso\Session':
        return \sso\config::$datastore->loadSession(...$id);
      case 'Authorization':
      case 'sso\Authorization':
      case '\sso\Authorization':
        return \sso\config::$datastore->loadAuthorization(...$id);
      default: throw new Exception("can't load objects of this type");
    }
  }
  public static function save(\sso\Session|\sso\Authorization $obj) : void {
    if($obj instanceof \sso\Session){
      \sso\config::$datastore->saveSession($obj);
    }else if($obj instanceof \sso\Authorization){
      \sso\config::$datastore->saveAuthorization($obj);
    }
  }
  public static function delete(\sso\Session|\sso\Authorization $obj){
    if($obj instanceof \sso\Session){
      \sso\config::$datastore->deleteSession($obj);
    }else if($obj instanceof \sso\Authorization){
      \sso\config::$datastore->deleteAuthorization($obj);
    }
  }
  public static function loadSessionOfUser(string $user) : array {
    return \sso\config::$datastore->loadSessionOfUser($user);
  }
  public static function loadAuthorizationOfSession(\sso\Session|string $session) : array {
    return \sso\config::$datastore->loadAuthorizationOfSession($session);
  }
  public static function logout(string $user) : void {
    \sso\config::$datastore->deleteSessionOfUser($user);
  }
  public static function check_password(string $user, string $password) : bool {
    $time = time();
    if(!\sso\config::$passwordstore->check($user, $password)){
      $diff = 5 - (time() - $time);
      if($diff > 0) sleep($diff + rand(0,2));
      return false;
    }
    return true;
  }
  public static function save_password(string $user, string $password) : bool {
    return \sso\config::$passwordstore->save($user, $password);
  }
}
