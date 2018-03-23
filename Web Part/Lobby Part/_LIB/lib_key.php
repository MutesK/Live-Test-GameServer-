<?php

  function MakeSessionKey()
  {
    srand(time() / 500);
    $characters = '0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-*=-';
    $strlen = strlen($characters) - 1;
    $SessionKey  = '';
    for($i = 0; $i < 32; $i++)
    {
      $SessionKey .= $characters[rand(0, $strlen)];
    }

    return  $SessionKey;
  }
?>