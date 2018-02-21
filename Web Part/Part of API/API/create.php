<?php

/*
    1. 이메일을 기준으로 AccountNo와 DBNO를 할당받는다.
    2. 그리고 해당 DBNO에 맞는 DB에 연결하여 해당 샤딩 스키마 안의 Account / Content 테이블에 Insert 한다.

    읽기용 DB, 쓰기용 DB가 분리되어야 함.
*/

include "_Startup.php";
include "_ResultCode.php";


function GetDBNO()
{
    global $ShSlave_DB;
    global $ResultCode;
    
    $ShSlave_DB->DB_SwitchDBName("shdb_info");

    $Query = "SELECT dbno from available where available = (select Max(available) from available)";
    $DBResult = $ShSlave_DB->DB_ExecQuery($Query);
    
    $Result = mysqli_fetch_array($DBResult, MYSQLI_ASSOC);
    mysqli_free_result($DBResult);
    
    $DBNO = $Result['dbno'];

    return $DBNO;
}

function UpdateInfomation($Email, $DBNO)
{
   global $ShMaster_DB;
   global $ResultCode;

   ////////////////////// 이파트 이미 존재하는지 확인해야된다.
   $ShMaster_DB->DB_SwitchDBName("shdb_index");
   $Query = "INSERT INTO allocate (email, dbno) VALUES ('{$Email}', '{$DBNO}')";
   $DBResult = $ShMaster_DB->DB_ExecQuery($Query);

   if($DBResult == false)
   {
        $ResultCode = df_ALREADY_JOINED_EMAIL;
        return false;
   }

   $AccountNo = mysqli_insert_id($ShMaster_DB->GetDBObject());

   // 해당 DBNO의 Avaiable 수치를 감소
   $ShMaster_DB->DB_SwitchDBName("shdb_info");
   $Query = "UPDATE available SET available = available - 1 WHERE dbno = $DBNO";
   $DBResult = $ShMaster_DB->DB_ExecQuery($Query);

   return $AccountNo;

}

function GetDBConnectInfo($DBNO)
{
    global $ShSlave_DB;

    $ShSlave_DB->DB_SwitchDBName("shdb_info");
    $Query = "SELECT * from dbconnect where dbno = $DBNO";
    $DBResult = $ShSlave_DB->DB_ExecQuery($Query);

    $Result = mysqli_fetch_array($DBResult, MYSQLI_ASSOC);
    mysqli_free_result($DBResult);

    return $Result;
}
//////////////////////////////////////////////////////////////////////////////////////////////


$ResultCode = df_RESULT_SUCCESS;

if(!isset($g_JSON['email']))
    $ResultCode = df_PARAMETER_ERROR;

if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;

    ResponseJSON($ResJSON);
    exit;
}

$email = $g_JSON['email'];
//---------------------------------------------
// DB 객체 
// 데이터 DB 객체는 각각 해당 프로그래밍 코드에서 생성 및 해제 해줘야됨.
//---------------------------------------------
$ShMaster_DB = new DBConnector("172.16.2.1", "GameServer", "!ss640803", "shdb_index");

// Slave IP는 2개 172.16.1.1, 172.16.1.2 랜덤 선택하게 해줘야됨.
$ShSlave_DB = new DBConnector("172.16.1.1", "GameServer", "!ss640803", "shdb_index");

if(!$ShMaster_DB->DBConnect())
    $ResultCode = df_DB_INDEX_MASTER_CONNECT_ERROR;

if(!$ShSlave_DB->DBConnect())
    $ResultCode = df_DB_INDEX_SLAVE_CONNECT_ERROR;

// 1. shdb_info의 Available에서 Available이 큰 dbno를 가져온다.
if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;

    ResponseJSON($ResJSON);
    //LOG_System($g_AccountNo, "{$_SERVER['PHP_SELF']}", "DEBUG # ResponseJSON = {$ResJSON}", 5);
    exit;
}

$DBNO = GetDBNO();

// 2. Master로 삽입함으로서 AccountNo를 가져온다. 그리고 해당 DBNO의 Avaiable 수치를 감소시킨다.
if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;

    ResponseJSON($ResJSON);
    //LOG_System($g_AccountNo, "{$_SERVER['PHP_SELF']}", "DEBUG # ResponseJSON = {$ResJSON}", 5);
    exit;
}

$AccountNo = UpdateInfomation($email, $DBNO);

// 3. 해당 DBNO 정보를 가져온다.
if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;

    ResponseJSON($ResJSON);

    exit;
}

$DBConnectArray = GetDBConnectInfo($DBNO);

// 4. 해당 정보 접속하는 DB 객체 생성
if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;

    ResponseJSON($ResJSON);

    exit;
}

$DBData = new DBConnector($DBConnectArray['ip'], $DBConnectArray['id'], 
$DBConnectArray['pass'], $DBConnectArray['dbname'], $DBConnectArray['port']);

if(!$DBData->DBConnect())
    $ResultCode = df_DB_DATA_CONNECT_ERROR;

// 5. 해당 AccountNo로 Insert 하여 공간 확보
if($ResultCode != df_RESULT_SUCCESS)
{
    $ResJSON = array();
    $ResJSON['result'] = $ResultCode;

    ResponseJSON($ResJSON);
  
    exit;
}

$qryarr = array();
array_push($qryarr, "INSERT INTO account (accountno, email) VALUES ('{$AccountNo}', '{$email}')");
array_push($qryarr, "INSERT INTO contents (accountno) VALUES ('{$AccountNo}')");

$DBData->DB_TransectionQuery($qryarr);

$ResJSON = array();
$ResJSON['result'] = $ResultCode;
$ResJSON['accountno'] = $AccountNo;
$ResJSON['email'] = $email;
$ResJSON['dbno'] = $DBNO;

ResponseJSON($ResJSON);

$ShMaster_DB->DBDisconnect();
$ShSlave_DB->DBDisconnect();
$DBData->DBDisconnect();

include "_Cleanup.php";
?>