<?php
/*
    Matchmaking Status DB 서버 정보를 활용하여 적절한 매치메이킹 서버 정보를 가져온다.

    DB에 반영은 없음, Slave 서버를 통해 가장 사용자가 적고 Timeout 시간 시내에 갱신된 서버를 얻음.

    TimeOut 시간은 별도의 설정 php를 통해서 간단히 수정 할수 있도록 함.
    현재는 메치메이킹 서버의 갱신이 없으므로, 999:00:00 처럼 엄청나게 긴 시간으로 설정하여 테스트
    
    사용자는 AccountNo, SessionKey 를 가지고 오며, Lobby Server에서는 shdb API를 사용하여 SessionKey를
    조회하려 인증 확인한다.
*/
include "_Startup.php";
include "Common.php";

$ResultCode = df_RESULT_SUCCESS;

if(!isset($g_JSON['accountno']) || !isset($g_JSON['sessionkey']))
    $ResultCode = df_PARAMETER_ERROR;

if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;

    ResponseJSON($ResJSON);
    LOG_System(-1, "Get_MatchMaking", "Parameter_ERROR");
    exit;
}

$AccountNo = $g_JSON['accountno'];
$SessionKey = $g_JSON['sessionkey'];

// API 호출 하여 데이터를 가져온다.
$APICallArr = array();
$APICallArr['accountno'] = $AccountNo;
$Json = json_encode($APICallArr);


$ResultJSON = http_request_JSON_sync($API['selectAccount'], $Json);
if($ResultJSON === 0)
    $ResultCode = df_SHDBAPI_HTTP_ERROR;


if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;
    
    ResponseJSON($ResJSON);
    LOG_System($AccountNo, "Get_MatchMaking", "Shading DB API HTTP POST ERROR");
    exit;
}

// SHDB에서 받은 JSON 으로 결과를 분석 (SessionKey 확인한다.)
$ResultJSON = json_decode($ResultJSON, true);

switch($ResultJSON['result'])
{
    case -11:
        $ResultCode = df_RETRY;
    break;
    case -10:
        $ResultCode = df_NOTEXIST_ACCOUNT;
    break;
    case 1:
        $ResultCode = df_RESULT_SUCCESS;
    break;
    default:
        $ResultCode = df_SHDBAPI_ETC_ERROR;
    break;
}

if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;
    
    ResponseJSON($ResJSON);
    LOG_System($AccountNo, "Get_MatchMaking", "ResultCode : ". $ResultCode);
    exit;
}

if(strcasecmp($ResultJSON['sessionkey'], $SessionKey) != 0)
    $ResultCode = df_AUTH_ERROR;


if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;
    
    ResponseJSON($ResJSON);
    LOG_System($AccountNo, "Get_MatchMaking", "SessionKey isn't Matched Request :". $SessionKey.", JSON Result :".$ResultJSON['sessionkey']);
    exit;
}
// Slave DB를 통해 메치메이킹 DB 테이블을 가져와서 TimeOut 시간보다 작은 서버리스트로 
// 매칭 하게끔한다.
$Rand = mt_rand(0, 1);
$IP = "";
switch($Rand)
{
    case 0:
        $IP = "P:172.16.1.1";
    break;
    case 1:
        $IP = "P:172.16.1.2";
    break;
}
$SlaveDB = new DBConnector($IP, "GameServer", "!ss640803", "matchmaking_status");
$SlaveDB->DBConnect();

$Query = "SELECT * from  matchmaking_status.server where timediff(now(), `heartbeat`) < \"$TimeOut\"";
$DBResult = $SlaveDB->DB_ExecQuery($Query);
$Result = mysqli_fetch_array($DBResult, MYSQLI_ASSOC);
mysqli_free_result($DBResult);

if($Result == 0)
    $ResultCode = df_NOSERVER;

$ResJSON = array();
$ResJSON['result'] = $ResultCode;
$ResJSON['serverno'] = $Result['serverno'];
$ResJSON['ip'] = $Result['ip'];
$ResJSON['port'] =$Result['port'];

ResponseJSON($ResJSON);


if(isset($SlaveDB))
    $SlaveDB->DBDisconnect();

include "_Cleanup.php";
?>