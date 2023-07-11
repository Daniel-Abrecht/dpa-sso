<?php

if(@$_GET['token']){
  setcookie("sso-token", $_GET['token'], [
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
