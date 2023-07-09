<?php require("header.php");
$error = [];
$success = false;
if( isset($_POST['oldpw' ])
 && isset($_POST['newpw' ])
 && isset($_POST['newpw2'])
){
  if(!\sso::check_password($login->user, $_POST['oldpw']))
    $error[] = "Altes Passwort stimmt nicht";
  if(!$_POST['newpw'])
    $error[] = "Neues Passwort kann nicht leer sein";
  if($_POST['newpw'] !== $_POST['newpw2'])
    $error[] = "Passwort stimmt nicht mit vorheriger eingabe überein!";
  if(!count($error)){
    if(\sso::save_password($login->user, $_POST['newpw'])){
      $success = true;
    }else{
      $error[] = 'Das neue Passwort konnte nicht gespeichert werden';
    }
  }
}
?>
<h1>Passwort ändern</h1>
<?php
if(count($error)){
?>
  <div class="infobox error">
    Es ist ein Fehler aufgetreten:
    <ul>
      <?php foreach($error as $e){ ?>
        <li><?php echo htmlentities($e); ?></li>
      <?php } ?>
    </ul>
  </div>
<?php
}
if($success){
?>
  <div class="infobox ok">
    Passwort wurde erfolgreich geändert
  </div>
<?php
}else{
?>
<form method="POST" action="?">
  <label>
    <span>Altes Passwort</span>
    <input type="password" name="oldpw" required />
  </label>
  <label>
    <span>Neues Passwort</span>
    <input type="password" name="newpw" required />
  </label>
  <label>
    <span>Neues Passwort wiederholen</span>
    <input
      type="password" name="newpw2" required
      oninput="this.setCustomValidity(this.form.elements.newpw.value === this.value ? '' : 'Passwort stimmt nicht mit vorheriger eingabe überein!')"
    />
  </label>
  <div><input type="submit" value="Speichern" /></div>
</form>
<?php } ?>
<?php require("footer.php"); ?>
