<?php require("sso/sso.php"); ?>

<h1>Hello <?php echo htmlentities(ucfirst(\sso\sso::$user)); ?></h1>

You are logged in!
