CREATE DATABASE  IF NOT EXISTS `nx_statistics` /*!40100 DEFAULT CHARACTER SET utf8 */;
USE `nx_statistics`;
-- MySQL dump 10.13  Distrib 5.5.41, for debian-linux-gnu (x86_64)
--
-- Host: 127.0.0.1    Database: nx_statistics
-- ------------------------------------------------------
-- Server version	5.5.41-0ubuntu0.14.04.1

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `cameras`
--

DROP TABLE IF EXISTS `cameras`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `cameras` (
  `id` varchar(45) NOT NULL,
  `parentId` varchar(45) NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `etc` text,
  `audioEnabled` tinyint(1) DEFAULT NULL,
  `controlEnabled` tinyint(1) DEFAULT NULL,
  `dewarpingParams` varchar(45) DEFAULT NULL,
  `firmware` varchar(45) DEFAULT NULL,
  `manuallyAdded` tinyint(1) DEFAULT NULL,
  `maxArchiveDays` int(11) DEFAULT NULL,
  `minArchiveDays` int(11) DEFAULT NULL,
  `model` varchar(45) DEFAULT NULL,
  `motionMask` varchar(45) DEFAULT NULL,
  `motionType` varchar(45) DEFAULT NULL,
  `scheduleEnabled` varchar(45) DEFAULT NULL,
  `secondaryStreamQuality` varchar(45) DEFAULT NULL,
  `status` varchar(45) DEFAULT NULL,
  `statusFlags` varchar(45) DEFAULT NULL,
  `vendor` varchar(45) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `clients`
--

DROP TABLE IF EXISTS `clients`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `clients` (
  `id` varchar(45) NOT NULL,
  `parentId` varchar(45) NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `etc` text,
  `cpuArchitecture` varchar(45) DEFAULT NULL,
  `cpuModelName` varchar(45) DEFAULT NULL,
  `phisicalMemory` int(64) DEFAULT NULL,
  `openGLRenderer` varchar(45) DEFAULT NULL,
  `openGLVendor` varchar(45) DEFAULT NULL,
  `openGLVersion` varchar(45) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `mediaservers`
--

DROP TABLE IF EXISTS `mediaservers`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `mediaservers` (
  `id` varchar(45) NOT NULL,
  `systemId` varchar(45) NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `etc` text,
  `allowAutoRedundancy` tinyint(1) DEFAULT NULL,
  `cpuArchitecture` varchar(45) DEFAULT NULL,
  `cpuModelName` varchar(45) DEFAULT NULL,
  `phisicalMemory` bigint(20) unsigned DEFAULT NULL,
  `flags` varchar(45) DEFAULT NULL,
  `maxCameras` varchar(45) DEFAULT NULL,
  `not_used` varchar(45) DEFAULT NULL,
  `status` varchar(45) DEFAULT NULL,
  `systemInfo` varchar(45) DEFAULT NULL,
  `version` varchar(45) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Table structure for table `storages`
--

DROP TABLE IF EXISTS `storages`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!40101 SET character_set_client = utf8 */;
CREATE TABLE `storages` (
  `id` varchar(45) NOT NULL,
  `parentId` varchar(45) NOT NULL,
  `timestamp` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `etc` text,
  `spaceLimit` bigint(20) DEFAULT NULL,
  `usedForWriting` tinyint(1) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;
/*!40101 SET character_set_client = @saved_cs_client */;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2015-03-26 20:21:30
