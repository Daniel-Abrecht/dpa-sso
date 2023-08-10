<!DOCTYPE html>
<html>
  <head>
    <title>SSO Login</title>
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <meta name="color-scheme" content="light dark">
    <link rel="stylesheet" type="text/css" href="style.css" />
    <style>
body {
  display: flex;
  align-items: center;
  justify-content: center;
}

input[id="permanent"]:not(:checked) ~ * label[for="permanent"],
input[id="1h"]:not(:checked) ~ * label[for="1h"],
input[id="today"]:not(:checked) ~ * label[for="today"] {
  background-color: #AA88FF88;
}
input[id="permanent"]:focus ~ * label[for="permanent"],
input[id="1h"]:focus ~ * label[for="1h"],
input[id="today"]:focus ~ * label[for="today"] {
  background-color: #AA88FFCC;
  box-shadow: 0 0 0.4em 0.2em #3AC;
}

h1 { text-align: center; }
</style>
  </head>
  <body>
    <form method="POST" action="#">
      <h1>SSO Login</h1>
      <?php if($referer??false){ ?>
        <div>Für: <?php echo htmlentities($referer); ?></div>
      <?php } ?>
      <label>
        <span>Benutzername</span>
        <span><input type="text" name="user" tabindex="1" required /></span>
      </label>
      <label>
        <span>Password</span>
        <span><input type="password" name="password" tabindex="2" required /></span>
      </label>
      <input type="radio" id="permanent" name="mode" value="inf" tabindex="5" required />
      <input type="radio" id="today" name="mode" value="today" tabindex="4" required />
      <input type="radio" id="1h" name="mode" value="1h" tabindex="3" required />
      <span>
        <label for="permanent">Dauerhaft</label>
        <label for="today">Heute</label>
        <label for="1h">1h</label>
      </span>
      <div><input type="submit" value="Login" tabindex="6" /></div>
      <input type="hidden" name="referer" value="<?php echo htmlentities($referer??''); ?>" /></div>
    </form>
  </body>
</html>
