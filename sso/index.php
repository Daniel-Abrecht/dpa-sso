<?php

if(isset($_COOKIE['dpa-sso-token'])){
  header("HTTP/1.1 403 Forbidden");
  echo "This location isn't supposed to be called with the dpa-sso-token cookie set, because it would allow overriding valid tokens.";
  exit();
}

if(@$_GET['token']){
  setcookie("dpa-sso-token", $_GET['token'], [
    'expires' => time()+1*60*60,
    'path' => '/',
    'secure' => true,
    'httponly' => true,
    'samesite' => 'None'
  ]);
}

if(@$_GET['location']){
  $location = $_GET['location'];
  if(!str_starts_with($location, '/'))
    $location = '/' . $location;
  header("HTTP/1.1 303 See Other");
  header("Location: $location");
}
