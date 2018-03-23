<?php

$API = array(
    "create" => "http://172.16.2.2/API/create.php",
    "selectAccount" => "http://172.16.2.2/API/select_account.php",
    "selectContents" => "http://172.16.2.2/API/select_contents.php",
    "updateAccount" => "http://172.16.2.2/API/update_contents.php",
    "updateContents" => "http://172.16.2.2/API/update_contents.php",
);


define("df_RESULT_SUCCESS",				1);

define("df_PARAMETER_ERROR",			    -100);
define("df_SHDBAPI_HTTP_ERROR",			    -110);
define("df_SHDBAPI_ETC_ERROR",			    -110);

define("df_RETRY",			    -1);
define("df_NOTEXIST_ACCOUNT",			    -2);
define("df_AUTH_ERROR",			    -3);
define("df_NOSERVER",			    -4);

$TimeOut = "999:00:00";

?>