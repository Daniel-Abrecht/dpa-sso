<?php require("auth.php");
?><!DOCTYPE html>
<html>
  <head>
    <title>SSO</title>
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <meta name="color-scheme" content="light dark">
    <link rel="stylesheet" type="text/css" href="style.css" />
  </head>
  <body>
    <header>
      <a href=".">SSO: <?php echo htmlentities($login->user); ?></a>
      <a href="pwchange.php">Passwort ändern</a>
      <form method="post" action="." class="right"><input type="submit" name="logout" value="Logout"/></form>
    </header>
    <main>
