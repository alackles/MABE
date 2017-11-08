# QM.py takes a list of variables and conditions an generates the calls to
# run MABE on all of the combinations of conditions.
#
# variables is a dictonary with abriviated names and a list of values to iterate over
# varNames is a dictonary associating the actual names in MABE with the abriviated names
# varList is a list of abriviated names, which determines the order of the names

# An Exceptions list can be defined to list combinations which you do not wish to run.
# i.e. if "A" = [0,1] and "B" = [0,1], but you do not wish to run A = 0 and B = 0;
# then define: Exceptions = [["A", 0, "B", 0],[...],...]
# You may define any number of exeptions. If a condition meets all of the rules in an
# exception, then that condition will be excuded from all runs.

# Once all combinations have been generated, MQ proceedes to launch each condition lastRep-firstRep+1
# times. That is condition_1_101, condition_1_102, ... using the rep number as the random seed.

# STRING VALUES vs. NUMERIC VALUES
# you may use string or numeric values for any numeric variable values. If you use numeric values, be
# aware that the formating may not be what you expect (i.e. removal of leading or trailing 0s) so you
# may find that string values are preferable.

from subprocess import call
from subprocess import Popen as callNoWait
import glob
import argparse
import os
import sys
#import pwd
import shutil
import datetime
import getpass  # for getuser() gets current username
import itertools # for generating combinations of conditions
import re

ptrnCommand = re.compile(r'^\s*([A-Z]+)\s') # ex: gets 'VAR' from 'VAR = UD GLOBAL-msg "a message"'
ptrnSpaceSeparatedEquals = re.compile(r'\s(\S*\".*\"|[^\s]+)') # ex: gets ['=','UD','GLOBAL-updated','a message']
ptrnCSVs = re.compile(r'\s*,?\s*([^\s",]+|\"([^"\\]|\\.)*?\")\s*,?\s*') # ex: gets ['1.2','2',"a \"special\" msg"] from '1.2,2,"a \"special\" msg"'

def makeQsubFile(realDisplayName, conditionDirectoryName, rep, qsubFileName, executable, cfg_files, workDir, conditions):
    outFile = open(qsubFileName, 'w')
    outFile.write('#!/bin/bash -login\n')
    for p in HPCC_parameters:
        outFile.write(p + '\n')
    outFile.write('#PBS -o ' + realDisplayName + '.out\n')
    outFile.write('#PBS -N ' + realDisplayName + '\n')

    outFile.write('\n' +
                  'shopt -s expand_aliases\n' +
                  'module load powertools\n' +
                  'module load GNU/6.2\n' +
                  '\n' +
                  'cd ' + workDir +
                  '\n')

    includeFileString = ""
    if (len(cfg_files) > 0):
        includeFileString += "-f "
        for fileName in cfg_files:
            includeFileString += fileName + ' '

    if HPCC_LONGJOB:
        outFile.write('# 4 hours * 60 minutes * 6 seconds - 60 seconds * 20 minutes\n' +
                      'export BLCR_WAIT_SEC=$(( 4 * 60 * 60 - 60 * 20 ))\n' +
                      #'export BLCR_WAIT_SEC=$( 30 * 60 )\n'+
                      'export PBS_JOBSCRIPT="$0"\n' +
                      '\n' +
                      'longjob ' + executable + ' ' + includeFileString + '-p GLOBAL-outputDirectory ' + conditionDirectoryName + '/' + str(rep) + '/ GLOBAL-randomSeed ' + str(rep) + ' ' + conditions + '\n')
    else:
        outFile.write(executable + ' ' + includeFileString + '-p GLOBAL-outputDirectory ' +
                      conditionDirectoryName + '/' + str(rep) + '/ GLOBAL-randomSeed ' + str(rep) + ' ' + conditions + '\n')
    outFile.write('ret=$?\n\n' +
                  'qstat -f ${PBS_JOBID}\n' +
                  '\n' +
                  'exit $ret\n')
    outFile.close()


parser = argparse.ArgumentParser()
parser.add_argument('-n', '--runNo', action='store_true', default=False,
                    help='if set, will do everything (i.e. create files and directories) but launch jobs, allows you to do a dry run - default : false(will run)', required=False)
parser.add_argument('-l', '--runLocal', action='store_true', default=False,
                    help='if set, will run jobs localy - default : false(no action)', required=False)
parser.add_argument('-d', '--runHPCC', action='store_true', default=False,
                    help='if set, will deploy jobs with qsub on HPCC - default : false(no action)', required=False)
parser.add_argument('-f', '--file', type=str, metavar='FILE_NAME', default='MQ_conditions.txt',
                    help='file which defines conditions - default: MQ_conditions.txt', required=False)
args = parser.parse_args()

variables = {}
variablesNonConditionsVersion = {}
varNames = {}
varList = []
exceptions = []
condition_sets = []
condition_sets_skipped = []
constantDefs = ' '
cfg_files = []
other_files = []
executable = "./MABE"
HPCC_parameters = []
HPCC_LONGJOB = True
using_conditions = False ## either use VAR/EXCEPT or CONDITIONS, but not both. Use of condition values overrides VAR values.

with open(args.file) as openfileobject:
    for rawline in openfileobject:

        line = rawline.split()
        if (len(line) > 0):
            if line[0] == "REPS":
                firstRep = int(line[2])
                lastRep = int(line[3])
            if line[0] == "JOBNAME":
                displayName = line[2]
                if displayName == "NONE":
                    displayName = ""
            if line[0] == "VAR": # VAR = PUN GLOBAL-poisonValue 0.0,1.0,1.5
                everythingEqualsAndAfterAsList = ptrnSpaceSeparatedEquals.findall(rawline) # 0:'=',1:variable,2:MABE-variable,3:values
                if everythingEqualsAndAfterAsList[0] is not '=':
                    print("error: VARs require an assignment for readability. Ex: CONDITIONS = TSK=1.0")
                    exit()
                var,mabeVar = everythingEqualsAndAfterAsList[1:3] # get variable and mabe-variable
                varList.append(var)
                varNames[var] = mabeVar
                if len(everythingEqualsAndAfterAsList) > 3: # allow for users to not specify any values
                    variables[var] = everythingEqualsAndAfterAsList[3]
                    variablesNonConditionsVersion[var] = [e[0] for e in ptrnCSVs.findall(variables[var])]
                else:
                    using_conditions = True # can't use standard VAR/EXCEPT when you don't specify values
            if line[0] == "EXCEPT": # EXCEPT = UH=1,UI=1
                everythingEqualsAndAfterAsList = ptrnSpaceSeparatedEquals.findall(rawline) # 0:'=',1:variable,2:MABE-variable,3:values
                if everythingEqualsAndAfterAsList[0] is not '=':
                    print("error: EXCEPT requires an assignment for readability. Ex: CONDITIONS = TSK=1.0")
                    exit()
                new_skip_condition_set = []
                for eachVar in everythingEqualsAndAfterAsList[1:]:
                    if eachVar.count('=') > 1:
                        print("error: more than 1 '=' character found in EXCEPT values (probably in a string?) and we haven't considered this problem yet.")
                        sys.exit()
                    variable,rawValues=eachVar.split('=')
                    values = [e[0] for e in ptrnCSVs.findall(rawValues)]
                    new_skip_condition_set.append([variable]+values)
                condition_sets_skipped.append(new_skip_condition_set) # results as: condition_sets=[[['PUN','0.0','1.0','1.5'], ['UH','1'], ['UI','1']], /* next condition set here... */ ]
            if line[0] == "CONDITIONS": # CONDITIONS = PUN=0.0,1.0,1.5;UH=1;UI=1
                using_conditions = True
                everythingEqualsAndAfterAsList = ptrnSpaceSeparatedEquals.findall(rawline) # 0:'=',1:variable,2:MABE-variable,3:values
                if everythingEqualsAndAfterAsList[0] is not '=':
                    print("error: CONDITIONS require an assignment for readability. Ex: CONDITIONS = TSK=1.0")
                    exit()
                new_condition_set = []
                for eachVar in everythingEqualsAndAfterAsList[1:]:
                    if eachVar.count('=') > 1:
                        print("error: more than 1 '=' character found in CONDITIONS values (probably in a string?) and we haven't considered this problem yet.")
                        sys.exit()
                    variable,rawValues=eachVar.split('=')
                    values = [e[0] for e in ptrnCSVs.findall(rawValues)]
                    new_condition_set.append([variable]+values)
                condition_sets.append(new_condition_set) # results as: condition_sets=[[['PUN','0.0','1.0','1.5'], ['UH','1'], ['UI','1']], /* next condition set here... */ ]
            if line[0] == "EXECUTABLE":
                executable = line[2]
            if line[0] == "CONSTANT":
                constantDefs += ' '.join(line[2:])
            if line[0] == "SETTINGS":
                cfg_files = line[2].split(',')
                for f in cfg_files:
                    if not(os.path.isfile(f)):
                        print('settings file: "' + f +
                              '" seems to be missing!')
                        exit()
            if line[0] == "OTHERFILES":
                other_files = line[2].split(',')
                for f in other_files:
                    if not(os.path.isfile(f)):
                        print('other file: "' + f + '" seems to be missing!')
                        exit()
            if line[0] == "HPCC_LONGJOB":
                HPCC_LONGJOB = (line[2] == "TRUE")
            if line[0] == "HPCC_PARAMETERS":
                newParameter = ""
                for i in line[2:]:
                    newParameter += i + ' '
                HPCC_parameters.append(newParameter[:-1])


ex_names = []
for ex in exceptions:
    ex_index = 0
    while ex_index < len(ex):
        if ex[ex_index] not in ex_names:
            ex_names.append(ex[ex_index])
        ex_index += 2

cond_var_names=set()
for cond_set in condition_sets:
    for cond_def in cond_set:
        cond_var_names.add(cond_def[0]) # add the variable name part of the definition (always [0])

for ex_name in ex_names:
    found_ex_name = False
    for v in varList:
        if v == ex_name:
            found_ex_name = True
    if not found_ex_name:
        print('exception rules contain variable with name: "' +
              ex_name + '". But this variable is not defined. Exiting.')
        exit()

for cond_var_name in cond_var_names:
    if cond_var_name not in varList:
        print('conditions contains variable with name: "' +
              cond_var_name + '". But this variable is not defined. Exiting.')
        exit()

print("\nSetting up your jobs...\n")
for v in varList:
    if v in variablesNonConditionsVersion:
        print(v + " (" + varNames[v] + ") = " + str(variablesNonConditionsVersion[v]))

print("")

reps = range(firstRep, lastRep + 1)

lengthsList = []
indexList = []
for key in varList:
    if key in variables:
        lengthsList.append(len(variablesNonConditionsVersion[key]))
        indexList.append(0)

combinations = []
conditions = []

if not using_conditions:
    done = False
    print("excluding:")
    # iterate over all combinations using nested counter (indexList)
    while not done:
        varString = ""
        condString = ""
        keyCount = 0

        # for every key, look up varName and value for that key and add to:
        # varstring - the parameters to be passed to MABE
        # condString - the name of the output directory and job name
        for key in varList:
            varString += " " + varNames[key] + " " + \
                str(variablesNonConditionsVersion[key][indexList[keyCount]])
            condString += "_" + key + "_" + \
                str(variablesNonConditionsVersion[key][indexList[keyCount]]) + "_"
            keyCount += 1

        cond_is_ex = False
        if len(exceptions) > 0:
            for rule in exceptions:
                ruleIndex = 0
                rule_is_ex = True
                while ruleIndex < len(rule):
                    keyCount = 0
                    for key in varList:
                        if (rule[ruleIndex] == key) and (str(rule[ruleIndex + 1]) != str(variablesNonConditionsVersion[key][indexList[keyCount]])):
                            rule_is_ex = False
                        keyCount += 1
                    ruleIndex += 2
                if rule_is_ex is True:
                    cond_is_ex = True

        # if this condition is not an exception append this conditions values to:
        # combinations - a list with parameters combinations
        # conditions - a list of directory name/job identifiers
        if not cond_is_ex:
            combinations.append(varString)
            conditions.append(condString)
        else:
            print("  " + condString[1:-1])

        checkIndex = len(indexList) - 1

        # This block of code moves the index of the last element up by one. If it reaches max
        # then it moves the next to last elements index up by one. If that also reaches max,
        # then the effect cascades. If the first elements index is reaches max then done is
        # set to true and processing of conditions is halted.
        stillChecking = True
        while stillChecking:
            indexList[checkIndex] += 1
            if indexList[checkIndex] == lengthsList[checkIndex]:
                indexList[checkIndex] = 0
                if checkIndex == 0:
                    done = True
                    stillChecking = False
                else:
                    checkIndex -= 1
            else:
                stillChecking = False
elif using_conditions: # This section is for parsing the CONDITIONS lines
    # calculate skip sets
    skipCombinations = []
    for skip_set in condition_sets_skipped:
        assignmentsInCategories = []
        for condition_var in skip_set:
            varString = varNames[condition_var[0]]+' '
            category=[]
            for var_value in condition_var[1:]: # loop over all values, skipping the variable name
                category.append( varString+var_value ) # store MABE-like parameter assignment
            assignmentsInCategories.append(category)
        skipCombinations += itertools.product(*assignmentsInCategories)
    # calculate include sets
    for condition_set in condition_sets:
        assignmentsInCategories = []
        for condition_var in condition_set:
            varString = varNames[condition_var[0]]+' '
            condString = '_'+condition_var[0]+'_'
            category=[]
            for var_value in condition_var[1:]: # loop over all values, skipping the variable name
                category.append( (varString+var_value, condString+var_value+'_') ) # store MABE-like parameter assignment, and condition path strings
            assignmentsInCategories.append(category)
        allCombinations = itertools.product(*assignmentsInCategories)
        for each_combination in allCombinations: # each_combination is tuple: (mabe-like param assignment, condition string)
            matchesi = [0]*len(skipCombinations) # boolean mask for which skips sets match, assume all match
            for test_value in [e[0] for e in each_combination]:
                for sci,skip_combo in enumerate(skipCombinations):
                    if test_value in skip_combo:
                        matchesi[sci] += 1
            should_not_skip = True
            for sci,skip_combo in enumerate(skipCombinations):
                if matchesi[sci] == len(skip_combo):
                    should_not_skip = False
                    break
            if should_not_skip:
                conditions.append(''.join([e[1] for e in each_combination])) # store full condition path string for folder name generation
                combinations.append(' '+' '.join([e[0] for e in each_combination])) # store MABE-like full parameter string
        
print("")
print("including:")
for c in conditions:
    print("  " + c[1:-1])
print("")

print("the following settings files will be included:")
for f in cfg_files:
    print("  " + f)
print("")

if not (args.runLocal or args.runHPCC):
    print("")
    print("If you wish to run, use a run option (runLocal or runHPCC)")
    print("")


userName = getpass.getuser()
print(userName)
absLocalDir = os.getcwd()  # this is needed so we can get back to the launch direcotry
# turn path into list so we can trim of verything before ~
localDirParts = absLocalDir.split('/')
# this is the launch directory from ~/ : this command appends '~/' onto the remaining list with '/' between each element.
localDir = '~/' + '/'.join(localDirParts[1:])

pathToScratch = '/mnt/scratch/'  # used for HPCC runs

# This loop cycles though all of the combinations and constructs to required calls to
# run MABE.

for i in range(len(combinations)):
    for rep in reps:
        if (args.runLocal):
            # turn cgf_files list into a space separated string
            cfg_files_str = ' '.join(cfg_files)
            print("running:")
            print("  " + executable + " -f " + cfg_files_str + " -p GLOBAL-outputDirectory " +
                  conditions[i][1:-1] + "/" + str(rep) + "/ " + "GLOBAL-randomSeed " + str(rep) + " " + combinations[i][1:] + constantDefs)
            # make rep directory (this will also make the condition directory if it's not here already)
            call(["mkdir", "-p", displayName + "_" +
                  conditions[i][1:-1] + "/" + str(rep)])
            if not args.runNo:
                # turn combinations string into a list
                params = combinations[i][1:].split()
                call([executable, "-f"] + cfg_files + ["-p", "GLOBAL-outputDirectory", displayName +
                                                       "_" + conditions[i][1:-1] + "/" + str(rep) + "/", "GLOBAL-randomSeed", str(rep)] + params + constantDefs.split())
        if (args.runHPCC):
            # go to the local directory (after each job is launched, we are in the work directory)
            os.chdir(absLocalDir)
            if (displayName == ""):
                realDisplayName = "C" + \
                    str(i) + "_" + str(rep) + "__" + conditions[i][1:-1]
                conditionDirectoryName = "C" + \
                    str(i) + "__" + conditions[i][1:-1]
            else:
                realDisplayName = displayName + "_C" + \
                    str(i) + "_" + str(rep) + "__" + conditions[i][1:-1]
                conditionDirectoryName = displayName + \
                    "_C" + str(i) + "__" + conditions[i][1:-1]

            timeNow = str(datetime.datetime.now().year) + '_' + str(datetime.datetime.now().month) + '_' + str(datetime.datetime.now().day) + \
                '_' + str(datetime.datetime.now().hour) + '_' + str(
                    datetime.datetime.now().minute) + '_' + str(datetime.datetime.now().second)

            workDir = pathToScratch + userName + '/' + \
                realDisplayName + '_' + str(rep) + '__' + timeNow

            # this is where data files will actually be writen (on scratch)
            outputDir = workDir + '/' + \
                conditionDirectoryName + '/' + str(rep) + '/'

            # if there was already a workDir, get rid of it.
            if os.path.exists(workDir):
                shutil.rmtree(workDir)
            os.makedirs(outputDir)
            shutil.copy(executable, workDir)  # copy the executable to scratch
            for f in cfg_files:
                shutil.copy(f, workDir)  # copy the settings files to scratch
            for f in other_files:
                shutil.copy(f, workDir)  # copy other files to scratch

            # if the local conditions directory is not already here, make it
            if not(os.path.exists(conditionDirectoryName)):
                os.makedirs(conditionDirectoryName)

            # if there is already a link for this rep from the local conditions directory to scratch, remove it
            if os.path.exists(conditionDirectoryName + '/' + str(rep)):
                os.unlink(conditionDirectoryName + '/' + str(rep))

            # create local link from the local conditions directory to the rep directory in work directory
            os.symlink(workDir + '/' + conditionDirectoryName + '/' +
                       str(rep), conditionDirectoryName + '/' + str(rep))

            os.chdir(workDir)  # goto the work dir (on scratch)

            qsubFileName = "MQ.qsub"

            # make the qsub file on scratch
            makeQsubFile(realDisplayName=realDisplayName, conditionDirectoryName=conditionDirectoryName, rep=rep,
                         qsubFileName=qsubFileName, executable=executable, cfg_files=cfg_files, workDir=workDir, conditions=combinations[i][1:]+constantDefs)

            print("submitting:")
            print("  " + realDisplayName + " :")
            print("  workDir = " + workDir)
            print("  qsub " + qsubFileName)
            if not args.runNo:
                callNoWait(["qsub", qsubFileName])  # run the job

if args.runNo:
    print("")
    print("  You are using --runNo, so no jobs have been started!")
