
<?php
//-----------------------------------------------------------
// 시스템 로그 남기는 함수 / DB, 테이블 정보는 _Config.php 참고.
//
//------------------------------------------------------------
include "_LIB/lib_DB.php";

global $g_DB;
$cnf_LOG_LEVEL = 3;

DBConnect();

printf("AccountNo : %s, Action : %s, Message : %s, Level : %d \n", 
$_POST['AccountNo'], $_POST['Action'],$_POST['Message'],$_POST['Level']);

if(!isset($_POST['AccountNo']))
    $AccountNo = "None";
else 
    $AccountNo = mysqli_real_escape_string($g_DB, $_POST['AccountNo']);

if(!isset($_POST['Action']))
    $Action = "None";
else 
    $Action = mysqli_real_escape_string($g_DB, $_POST['Action']);

if(!isset($_POST['Message']))
    $Message = "None";
else 
    $Message = mysqli_real_escape_string($g_DB, $_POST['Message']);

if(!isset($_POST['Level']))
    $Level = "0";
else 
    $Level = mysqli_real_escape_string($g_DB, $_POST['Level']);

    
printf("AccountNo : %s, Action : %s, Message : %s, Level : %d \n", 
    $_POST['AccountNo'], $_POST['Action'],$_POST['Message'],$_POST['Level']);
    

if($Level < $cnf_LOG_LEVEL)
      return;

// 월별 테이블 이름 만들어서 로그 INSERT
$Query = "INSERT INTO `SystemLog_".date("Ym")."` (date, AccountNo, Action, Message)  VALUES (NOW(), \"$AccountNo\", \"$Action\", \"$Message\")";

$DBResult = mysqli_query($g_DB, $Query);


if(!$DBResult)
 {
    $errNo = mysqli_errno($g_DB);

    if($errNo == 1146)
    {
        $TableQuery = "CREATE TABLE `SystemLog_".date("Ym")."` LIKE SystemLog_template";

        mysqli_query($g_DB, $TableQuery);
        mysqli_query($g_DB, $Query);
    }
}

DBDisconnect();


?>













