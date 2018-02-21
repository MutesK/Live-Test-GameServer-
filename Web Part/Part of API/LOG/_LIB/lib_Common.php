<?php

  function QuitError($Error = "Error")
  {
    // 본래 여기에선 에러코드에 대응하는 문자열을 얻어주는 부분이 들어감

    $ErrorJson = array();
    $ErrorJson['resultcode'] = 0;
    $ErrorJson['resultmsg'] = $Error;

    DBDisconnect();

    exit(ResponseJSON($ErrorJson));
  }

  function ResponseJSON($JSONObject)
  {
    global $g_AccountNo;

    if(!isset($g_AccountNo))
      $g_AccountNo = -1;

    $resJSON = json_encode($JSONObject);

    LOG_System($g_AccountNo, "{$_SERVER['PHP_SELF']}", "DEBUG # ResponseJSON = {$resJSON}", 5);
    echo $resJSON;
  }



 ?>