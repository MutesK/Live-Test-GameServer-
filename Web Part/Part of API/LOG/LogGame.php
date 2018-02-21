<?php

/*
 예전 방식

    global $HTTP_RAW_POST_DATA; // 바디에 들어온걸 그대로 

    위 설정을 쓸려면 php.ini에서 always_populate_raw_post_data 가 1로 설정되어 있어야한다.

지금방식
    echo "php://input Type RAW DATA : ". file_get_contents("php://input");
*/


include "_LIB/lib_DB.php";


if(!isset($_POST['LogChunk']))
{
    echo 'Failed Post';
    exit;
}

DBConnect();

$RawData = json_decode($_POST['LogChunk']);

print_r($RawData);


$Query = "INSERT INTO `GameLog_".date("Ym")."` (date, AccountNo, LogType, LogCode, Param1, Param2, Param3, Param4, ParamString)  VALUES (NOW(), {$RawData->AccountNo},
{$RawData->Type}, \"{$RawData->Code}\", {$RawData->Param1}, {$RawData->Param2}, {$RawData->Param3}, {$RawData->Param4}, \"{$RawData->ParamString}\")";

echo $Query;

global $g_DB;
$DBResult = mysqli_query($g_DB, $Query);

if(!$DBResult)
{
    $errNo = mysqli_errno($g_DB);
    echo $errNo;

    if($errNo == 1146)
    {
        $TableQuery = "CREATE TABLE `GameLog_".date("Ym")."` LIKE GameLog_template";

        mysqli_query($g_DB, $TableQuery);
        mysqli_query($g_DB, $Query);
    }
}

DBDisconnect();


?>