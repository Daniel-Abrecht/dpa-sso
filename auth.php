<?php

require_once("sso.php");

if(!isset($login)){
  $login = null;
  if(isset($_COOKIE['sso-session']))
    $login = \sso::load(\sso\Session::class, $_COOKIE['sso-session']);
  if(!$login){
    header('HTTP/1.0 302 Unauthorized');
    header("Location: /");
    exit();
  }
}

$self_origin = 'https://' . $_SERVER['HTTP_HOST'] === $_SERVER['HTTP_ORIGIN'];
