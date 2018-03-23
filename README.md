
# 게임서버 

[TOC]

## 서버 구성도

## 서버 구동화면

+ 마스터서버

![마스터서버](https://github.com/MutesK/Live-Test-GameServer-NOT-COMPLETE-/blob/master/%EB%A7%88%EC%8A%A4%ED%84%B0%EC%84%9C%EB%B2%84.PNG?raw=true)

+ 매치서버

![매치서버](https://github.com/MutesK/Live-Test-GameServer-NOT-COMPLETE-/blob/master/%EB%A7%A4%EC%B9%98%EC%84%9C%EB%B2%84.PNG?raw=true)

+ 배틀서버

![배틀서버](https://github.com/MutesK/Live-Test-GameServer-NOT-COMPLETE-/blob/master/%EB%B0%B0%ED%8B%80%EC%84%9C%EB%B2%84.PNG?raw=true)

+ 채팅서버

![채팅서버](https://github.com/MutesK/Live-Test-GameServer-NOT-COMPLETE-/blob/master/%EC%B1%84%ED%8C%85%EC%84%9C%EB%B2%84.PNG?raw=true)


## DB

###Shading DB Table

1. Matchmaking Status

   > 메치메이킹 서버의 현황을 저장한다.

| serverno |    ip    | port | connectuser | heartbeat |
| :------: | :------: | :--: | :---------: | :-------: |
|   INT    | CHAR(20) | INT  |     INT     | TIMESTAMP |

```sql
DROP TABLE IF EXISTS `server`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `server` (
  `serverno` int(11) NOT NULL,
  `ip` char(20) DEFAULT NULL,
  `port` int(11) DEFAULT NULL,
  `connectuser` int(11) DEFAULT NULL,
  `heartbeat` timestamp NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`serverno`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
```

2. Shdb_index

   > 플레이어가 어디 DB번호에 있는지 확인하고 있다면 해당 DB 번호로 접속, 아니라면 기록

   | AccountNo | Email | DBNO |
   | :-------: | :---: | :--: |
   |  BIGINT   | CHAR  | INT  |

   ```sql
   DROP TABLE IF EXISTS `allocate`;
   /*!40101 SET @saved_cs_client     = @@character_set_client */;
   /*!40101 SET character_set_client = utf8 */;
   CREATE TABLE `allocate` (
     `accountno` bigint(20) NOT NULL AUTO_INCREMENT,
     `email` char(128) NOT NULL,
     `dbno` int(11) NOT NULL,
     PRIMARY KEY (`accountno`),
     UNIQUE KEY `email` (`email`),
     KEY `idx_email` (`email`)
   ) ENGINE=InnoDB AUTO_INCREMENT=57024 DEFAULT CHARSET=utf8;
   ```

   ​

3. Shdb_info

   1. available table

      	> 각 DBNO별 접속가능한 호스트 수를 기록한다.

   | DBNo | Available |
   | :--: | :-------: |
   | INT  |    INT    |

      ```sql
      DROP TABLE IF EXISTS `available`;
      /*!40101 SET @saved_cs_client     = @@character_set_client */;
      /*!40101 SET character_set_client = utf8 */;
      CREATE TABLE `available` (
        `dbno` int(11) NOT NULL,
        `available` int(11) NOT NULL,
        PRIMARY KEY (`dbno`)
      ) ENGINE=InnoDB DEFAULT CHARSET=utf8;
      ```

   2. dbconnect table

      > DB 번호와 DB 서버 정보를 저장한다.

      | DBNO |  IP  | PORT |  ID  | PASS | DBNAME |
      | :--: | :--: | :--: | :--: | :--: | :----: |
      | INT  | CHAR | INT  | CHAR | CHAR |  CHAR  |

      ```sql
      DROP TABLE IF EXISTS `dbconnect`;
      /*!40101 SET @saved_cs_client     = @@character_set_client */;
      /*!40101 SET character_set_client = utf8 */;
      CREATE TABLE `dbconnect` (
        `dbno` int(11) NOT NULL,
        `ip` char(16) DEFAULT NULL,
        `port` int(11) DEFAULT NULL,
        `id` char(30) DEFAULT NULL,
        `pass` char(30) DEFAULT NULL,
        `dbname` char(30) DEFAULT NULL,
        PRIMARY KEY (`dbno`)
      ) ENGINE=InnoDB DEFAULT CHARSET=utf8;
      ```

   ### Log DB 

   > 네이티브 C++ 서버에서 생기는 로그는 자체 텍스트로 로그를 남기고 있습니다.
   >
   > 이는 전부 웹에서 동작하는 모든 일련의 동작이나 문제를 기록합니다.

   1. GameLog

      > 웹 서버인 로그와 샤딩 API에서 생기는 게임로그를 기록한다.

      ```sql
      DROP TABLE IF EXISTS `gamelog_template`;
      /*!40101 SET @saved_cs_client     = @@character_set_client */;
      /*!40101 SET character_set_client = utf8 */;
      CREATE TABLE `gamelog_template` (
        `no` bigint(20) NOT NULL AUTO_INCREMENT,
        `date` datetime NOT NULL,
        `AccountNo` bigint(20) NOT NULL,
        `LogType` int(11) NOT NULL,
        `LogCode` int(11) NOT NULL,
        `Param1` int(11) DEFAULT '0',
        `Param2` int(11) DEFAULT '0',
        `Param3` int(11) DEFAULT '0',
        `Param4` int(11) DEFAULT '0',
        `ParamString` varchar(256) DEFAULT '',
        PRIMARY KEY (`no`,`AccountNo`)
      ) ENGINE=MyISAM DEFAULT CHARSET=utf8;
      ```

   2. Profiling Log

      > 웹 서버인 로비서버와  API에서의 성능기록

      ```sql
      DROP TABLE IF EXISTS `profilinglog_template`;
      /*!40101 SET @saved_cs_client     = @@character_set_client */;
      /*!40101 SET character_set_client = utf8 */;
      CREATE TABLE `profilinglog_template` (
        `no` bigint(20) NOT NULL AUTO_INCREMENT,
        `date` datetime NOT NULL,
        `IP` varchar(40) NOT NULL,
        `AccountNo` bigint(20) NOT NULL,
        `Action` varchar(128) DEFAULT NULL,
        `T_Page` float NOT NULL,
        `T_Mysql_Conn` float NOT NULL,
        `T_Mysql` float NOT NULL,
        `T_ExtAPI` float NOT NULL,
        `T_Log` float NOT NULL,
        `T_ru_u` float DEFAULT NULL,
        `T_ru_s` float DEFAULT NULL,
        `Query` text NOT NULL,
        `Comment` text NOT NULL,
        PRIMARY KEY (`no`)
      ) ENGINE=MyISAM DEFAULT CHARSET=utf8;
      ```

   3. System Log

      > API와 로비서버에서 생기는 시스템적 오류 기록	

      ```sql
      DROP TABLE IF EXISTS `systemlog_template`;
      /*!40101 SET @saved_cs_client     = @@character_set_client */;
      /*!40101 SET character_set_client = utf8 */;
      CREATE TABLE `systemlog_template` (
        `no` int(11) NOT NULL AUTO_INCREMENT,
        `date` datetime NOT NULL,
        `AccountNo` varchar(50) NOT NULL,
        `Action` varchar(128) DEFAULT NULL,
        `Message` varchar(1024) DEFAULT NULL,
        PRIMARY KEY (`no`,`date`,`AccountNo`)
      ) ENGINE=MyISAM DEFAULT CHARSET=utf8;
      ```


### 게임 컨텐츠 DB

> 해당 DB의 스키마의 이름은 DBConnect 테이블의 dbname 칼럼에 따라 설정해줘야 한다.

```sql
DROP TABLE IF EXISTS `account`;
CREATE TABLE `account` (
  `accountno` bigint(20) NOT NULL,
  `email` char(128) DEFAULT NULL,
  `nick` char(20) DEFAULT NULL,
  `password` char(128) DEFAULT NULL,
  `sessionkey` char(150) DEFAULT NULL,
  PRIMARY KEY (`accountno`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


DROP TABLE IF EXISTS `contents`;
CREATE TABLE `contents` (
  `accountno` bigint(20) NOT NULL,
  `playtime` int(10) unsigned DEFAULT NULL,
  `playcount` int(10) unsigned DEFAULT NULL,
  `kill` int(10) unsigned DEFAULT NULL,
  `die` int(10) unsigned DEFAULT NULL,
  `win` int(10) unsigned DEFAULT NULL,
  PRIMARY KEY (`accountno`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
```



------

## 웹 파트

1. 로비서버

   Matchmaking Status DB을 이용해서, 적절한 메치메이킹 정보를 가져온다.

   TIMEOUT 시간 시내에 갱신된 서버를 얻는다.

```json
// Request
// get_matchmaking.php로 POST 전송
{
    "accountno" : <계정번호>,
    "sessionkey": "xxxxxxxxxxxxxxxx"
}
// Response
{
    "result" : //
    "serverno" : <매치메이킹 서버 번호>
    "ip" : <매치메이킹 서버 주소>
    "port" : <매치메이킹 서버 포트>
}
```

2. Shading API 서버
   1. ID 신규할당

   > email 기준으로 새로운 accountno와 email을 받는다.

   ```json
   // create.php
   // Request
   {
       "email" : <가입할 이메일 주소>
   }
   // Response
   {
       "result" : //
       "accountno" :
       "email" : <가입한 이메일 주소>
       "dbno" : <접속할 DB>
   }
   ```

   2. 계정 저장

   > AccountNo 또는 이메일 기준으로 입력된 계정 저장
   >
   > 따로 추가할 내용은 그냥 JSON에 넣어주면된다. DB에 칼럼이 있다면 해당 칼럼으로 들어간다.

   ```json
   // update_account.php
   // request
   {
       "accountno" : 또는 "email" :
       "password" : 
       "sessionkey" :
       "nick" :
   }
   // Response
   {
       "result" : 
   }
   ```


   3. 데이터 저장

   ```json
   // update_account.php
   // request
   {
       "accountno" : 또는 "email" :
       "playtime" :
       "playcount" :
       "kill" :
       "die" :
       "win" :
   }
   // Response
   {
       "result" : 
   }
   ```

   4. 계정 읽기

      > accountno와 email 기준으로 회원 데이터를 읻어옴.

   ```json
   // select_account.php
   // request
   {
       "accountno" : 또는 "email" :
   }
   // Response
   {
   	"result" : 
   	"accountno" : 
   	"email" :
       "password" : 
       "sessionkey" :
       "nick" :
   }
   ```

   5. 데이터 읽기

      >  accountno와 email 기준으로 컨텐츠 데이터를 얻어옴.

   ```json
     // select_content.php
      // request
      {
          "accountno" : 또는 "email" :
      }
      // response
      {
          "result" : 
          "accountno" : 
          "playtime" :
          "playcount" :
          "kill" :
          "die" :
          "win" :
      }
   ```

### 에러코드

```php
// Shading API

define("df_RESULT_SUCCESS",				1);

define("df_ALREADY_JOINED_EMAIL",				-1);
define("df_DB_INDEX_ALLOCATE_ERROR",			-2);
define("df_DB_DATA_INSERT_ERROR",			    -3);

define("df_NONE_REGISTER",			            -10);
define("df_DATA_ACCOUNT_NOT_EXIST",			    -11);
define("df_DATA_CONTENTS_NOT_EXIST",			-12);


define("df_DB_INDEX_MASTER_CONNECT_ERROR",			            -50);
define("df_DB_INDEX_SLAVE_CONNECT_ERROR",			            -51);
define("df_DB_DATA_CONNECT_ERROR",			                    -52);

define("df_DB_QUERY_ERROR",			                    -60);
define("df_DB_UPDATE_COLUMN_ERROR",			            -61);
define("df_DB_UPDATESELECT_TABLE_ERROR",			    -62);


define("df_PARAMETER_ERROR",			    -100);

// 로비서버 

define("df_RESULT_SUCCESS",				1);

define("df_PARAMETER_ERROR",			    -100);
define("df_SHDBAPI_HTTP_ERROR",			    -110);
define("df_SHDBAPI_ETC_ERROR",			    -110);

define("df_RETRY",			    -1);
define("df_NOTEXIST_ACCOUNT",			    -2);
define("df_AUTH_ERROR",			    -3);
define("df_NOSERVER",			    -4);
```

