<?php

require_once("sso.php");

if(!isset($login)){
  $login = null;
  if(isset($_COOKIE['sso-session']))
    $login = \sso::load(\sso\Session::class, $_COOKIE['sso-session']);
  if(!$login){
    header('HTTP/1.0 302 Unauthorized');
    header("Cache-Control: no-store", true);
    header("Location: /");
    exit();
  }
}

if($_SERVER['REQUEST_METHOD'] !== 'GET' && $_SERVER['REQUEST_METHOD'] !== 'HEAD')
  header("Cache-Control: no-store", true);

$self_origin = 'https://' . $_SERVER['HTTP_HOST'] === ($_SERVER['HTTP_ORIGIN']??null);
