#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Sat Jul  8 01:06:28 2017

@author: user
"""
        

def isBuilding(idx):
    '''
    6 / Terran_Marine
    7 / Terran_Ghost
    8 / Terran_Vulture
    9 / Terran_Goliath
    10 / Terran_SCV
    11 / Terran_Wraith
    12 / Terran_Science_Vessel
    13 / Terran_Dropship
    14 / Terran_Battlecruiser
    15 / Terran_Nuclear_Missile
    16 / Terran_Siege_Tank_Siege_Mode
    17 / Terran_Firebat
    18 / Terran_Medic
    19 / Terran_Valkyrie
    20 / Terran_Command_Center
    21 / Terran_Comsat_Station
    22 / Terran_Nuclear_Silo
    23 / Terran_Supply_Depot
    24 / Terran_Refinery
    25 / Terran_Barracks
    26 / Terran_Academy
    27 / Terran_Factory
    28 / Terran_Starport
    29 / Terran_Control_Tower
    30 / Terran_Science_Facility
    31 / Terran_Covert_Ops
    32 / Terran_Machine_Shop
    33 / Terran_Engineering_Bay
    34 / Terran_Armory
    35 / Terran_Missile_Turret
    36 / Terran_Bunker
    '''
    assert idx >= 6 and idx <= 36
    
    if idx < 20:
        return False
    else:
        return True

def create_onehot(idx):
    rst = [0 for i in range(37)]
    rst[idx-6] = 1
    return rst
       
def createY(lines):
    all_rows = []
    for i in lines:
        all_rows.append(i.strip().split("|"))
        
        if all_rows[-1][1] == "Zerg":
            all_rows[-1][1] = '0'
        if all_rows[-1][1] == "Terran":
            all_rows[-1][1] = '1'
        if all_rows[-1][1] == "Protoss":
            all_rows[-1][1] = '2'
            
    int_all_rows = list(map(lambda x: list(map(int, x)), all_rows))
    
    #total supply > 0
    if int_all_rows[0][5] == 0:
        return None
    
    #global idle_rows
    #global command_rows
    command_rows = []
    skip_cnt = 0
    for i in range(len(int_all_rows)-1, 0, -1):
        if skip_cnt > 0:
            skip_cnt -= 1
            continue
        
		#아군 유닛 변화가 있을때만 Y로 저장
		#단, 건물은 바로 직전 row가 명령내리는 프레임이 맞는데...
		#유닛은 -2 row가 명령내리는 프레임으로 생각됨... 초반 빌드 빠지는걸 보면 그러함
		#그러나 유닛이 두개이상 찍힌 경우는 또 다름... ㅜㅜ
        same = True
        for j in range(6,37,1): #아군인덱스 6 ~ 36
            if all_rows[i][j] != all_rows[i-1][j]:
                if isBuilding(j):
                    command_rows.insert(0, (all_rows[i-1], j-6))
                    skip_cnt = max(skip_cnt, 2)
                    same = False
                else:
                    command_rows.insert(0, (all_rows[i-2], j-6))
                    skip_cnt = max(skip_cnt, 3)
                    same = False

        #if same:
        #    idle_rows.insert(0, all_rows[i])
    return command_rows

import os

base_dir =  'C:\\Users\\kimyujin\\Desktop\\AIC2017\\dataset_works\\dataset'
base_dir = 'C:\\StarCraft\\Maps\\replays\\dataset\\small\\1\\fin'
os.chdir(base_dir)

all_data = []
file_list = []
origin_lines = []
command_rows_lines = []
for i in os.listdir():
    if os.path.isdir(i): continue
    if os.path.splitext(i)[-1] != '.djm': continue

    with open(i, 'r') as f:
        tmp_all_lines = f.readlines()[1:]
        if tmp_all_lines[0].find("no terran") > -1:
            continue
        
        origin_lines.append(len(tmp_all_lines))
        tmp_command_rows = createY(tmp_all_lines)
        
        if tmp_command_rows is not None and len(tmp_command_rows) > 0:
            command_rows_lines.append(len(tmp_command_rows))
            all_data = all_data + tmp_command_rows
            


'''
import re
p = re.compile("^Frame")
p2 = re.compile("no terran")

header = all_lines[0]

for i in range(len(all_lines)-1, -1, -1):
    if p.match(all_lines[i]) is not None:
        all_lines.pop(i)
    elif p2.match(all_lines[i]) is not None:
        all_lines.pop(i)
'''
'''       
all_rows.sort()

for i in range(len(all_rows)-1, 0, -1):
    same = True
    for j in range(len(all_rows[i])):
        if all_rows[i][j] != all_rows[i-1][j]:
            same = False
            break
    if same:
        all_rows.pop(i)
'''
###############################
###############################
from matplotlib import pyplot
y, bins, _ = pyplot.hist(command_rows_lines)


#################################
#--------------------------합치기
#################################

with open("djm_only_changing_order2.dat", 'w') as f:
    #f.write(" ".join(header))
    for i in all_data:
        f.write(str(i[1]-6)+" "+" ".join(i[0])+"\n")
        
import numpy as np
a= np.loadtxt("djm_only_changing_order2.dat", dtype=np.int32, delimiter=' ', skiprows=1)
        

#################################
#--------------------------분석용
#################################33
tmp_all_rows = list(map(lambda x: list(map(int, x)), all_rows))

'''
for i in range(len(tmp_all_rows)):
    tmp_all_rows[i] = tmp_all_rows[i][-94:]
'''

sum_list = [0 for i in range(len(tmp_all_rows[0]))]

for i in tmp_all_rows:
    for j in range(len(i)):
        sum_list[j] += i[j]
        
header = header.strip().split("|")
for i in zip(header, sum_list):
    print(i)
    
    
'''
('Frame', 7841796150)
('Race', 602860)
('Mineral', 314685464)
('Gas', 229850962)
('SupplyUsed', 77052770)
('SupplyTotal', 91736000)
('Terran_Marine', 3802977)
('Terran_Ghost', 23079)
('Terran_Vulture', 2420757)
('Terran_Goliath', 1128507)
('Terran_SCV', 20158257)
('Terran_Wraith', 207055)
('Terran_Science_Vessel', 236143)
('Terran_Dropship', 187763)
('Terran_Battlecruiser', 0)
('Terran_Nuclear_Missile', 1033)
('Terran_Siege_Tank_Siege_Mode', 2666112)
('Terran_Firebat', 143177)
('Terran_Medic', 653347)
('Terran_Valkyrie', 9060)
('Terran_Command_Center', 1235474)
('Terran_Comsat_Station', 692039)
('Terran_Nuclear_Silo', 2002)
('Terran_Supply_Depot', 4540174)
('Terran_Refinery', 885135)
('Terran_Barracks', 949809)
('Terran_Academy', 364645)
('Terran_Factory', 1644999)
('Terran_Starport', 360158)
('Terran_Control_Tower', 265325)
('Terran_Science_Facility', 154389)
('Terran_Covert_Ops', 6011)
('Terran_Machine_Shop', 687737)
('Terran_Engineering_Bay', 376971)
('Terran_Armory', 275514)
('Terran_Missile_Turret', 1539088)
('Terran_Bunker', 184195)
('Terran_Marine', 34833)
('Terran_Ghost', 0)
('Terran_Vulture', 63549)
('Terran_Goliath', 110058)
('Terran_SCV', 1793346)
('Terran_Wraith', 21795)
('Terran_Science_Vessel', 0)
('Terran_Dropship', 42403)
('Terran_Battlecruiser', 0)
('Terran_Nuclear_Missile', 0)
('Terran_Siege_Tank_Siege_Mode', 258353)
('Terran_Firebat', 0)
('Terran_Medic', 0)
('Zerg_Larva', 161706)
('Zerg_Egg', 90156)
('Zerg_Zergling', 681238)
('Zerg_Hydralisk', 263092)
('Zerg_Ultralisk', 15922)
('Zerg_Drone', 2733330)
('Zerg_Overlord', 722393)
('Zerg_Mutalisk', 349930)
('Zerg_Guardian', 2027)
('Zerg_Queen', 0)
('Zerg_Defiler', 10909)
('Zerg_Scourge', 47527)
('Zerg_Infested_Terran', 0)
('Terran_Valkyrie', 154)
('Protoss_Corsair', 1365)
('Protoss_Dark_Templar', 30752)
('Zerg_Devourer', 0)
('Protoss_Dark_Archon', 0)
('Protoss_Probe', 6095125)
('Protoss_Zealot', 564216)
('Protoss_Dragoon', 1363028)
('Protoss_High_Templar', 20937)
('Protoss_Archon', 336)
('Protoss_Shuttle', 91884)
('Protoss_Scout', 11532)
('Protoss_Arbiter', 32302)
('Protoss_Carrier', 58927)
('Protoss_Reaver', 27071)
('Protoss_Observer', 326437)
('Zerg_Lurker', 269309)
('Terran_Command_Center', 141372)
('Terran_Comsat_Station', 43621)
('Terran_Nuclear_Silo', 0)
('Terran_Supply_Depot', 286838)
('Terran_Refinery', 107793)
('Terran_Barracks', 47523)
('Terran_Academy', 19657)
('Terran_Factory', 155230)
('Terran_Starport', 26377)
('Terran_Control_Tower', 14211)
('Terran_Science_Facility', 165)
('Terran_Covert_Ops', 0)
('Terran_Physics_Lab', 0)
('Terran_Machine_Shop', 78131)
('Terran_Engineering_Bay', 16888)
('Terran_Armory', 24829)
('Terran_Missile_Turret', 50322)
('Terran_Bunker', 1016)
('Zerg_Infested_Command_Center', 0)
('Zerg_Hatchery', 360751)
('Zerg_Lair', 79385)
('Zerg_Hive', 34940)
('Zerg_Nydus_Canal', 6500)
('Zerg_Hydralisk_Den', 92660)
('Zerg_Defiler_Mound', 16114)
('Zerg_Greater_Spire', 612)
('Zerg_Queens_Nest', 37779)
('Zerg_Evolution_Chamber', 90602)
('Zerg_Ultralisk_Cavern', 8680)
('Zerg_Spire', 74964)
('Zerg_Spawning_Pool', 124657)
('Zerg_Creep_Colony', 19069)
('Zerg_Spore_Colony', 18543)
('Zerg_Sunken_Colony', 276448)
('Zerg_Extractor', 212329)
('Protoss_Nexus', 484178)
('Protoss_Robotics_Facility', 108844)
('Protoss_Pylon', 1452149)
('Protoss_Assimilator', 280503)
('Protoss_Observatory', 108820)
('Protoss_Gateway', 665496)
('Protoss_Photon_Cannon', 230044)
('Protoss_Citadel_of_Adun', 68083)
('Protoss_Cybernetics_Core', 183088)
('Protoss_Templar_Archives', 48161)
('Protoss_Forge', 71361)
('Protoss_Stargate', 70146)
('Protoss_Fleet_Beacon', 16929)
('Protoss_Arbiter_Tribunal', 20508)
('Protoss_Robotics_Support_Bay', 44199)
('Protoss_Shield_Battery', 519)
'''

for i,v in enumerate(zip(header, sum_list)):
    if v[1] == 0:
        print("idx_{}:{}-{}".format(i,v[0], v[1]))
        
'''
idx_14:Terran_Battlecruiser-0
idx_38:Terran_Ghost-0
idx_43:Terran_Science_Vessel-0
idx_45:Terran_Battlecruiser-0
idx_46:Terran_Nuclear_Missile-0
idx_48:Terran_Firebat-0
idx_49:Terran_Medic-0
idx_59:Zerg_Queen-0
idx_62:Zerg_Infested_Terran-0
idx_66:Zerg_Devourer-0
idx_67:Protoss_Dark_Archon-0
idx_82:Terran_Nuclear_Silo-0
idx_91:Terran_Covert_Ops-0
idx_92:Terran_Physics_Lab-0
idx_98:Zerg_Infested_Command_Center-0
'''



