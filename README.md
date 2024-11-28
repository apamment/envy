# Envy BBS Documentation

The way I set up envy:

  1. Clone repo into home directory.
  2. create build & run subdirectories.
  3. build servo and envy in subdirectories of build.
  4. link them into run.
  5. create data, msgs and log directories in run.
  6. create gfiles directory in run and copy gfiles over (so i can customize without touching the repo).
  7. create envy.ini in run directory.
  8. run servo from run directory.

## Configuration Files
### envy.ini

`envy.ini` lives in your BBS's working directory, it is used both by servo and envy.

 * Main Section
   * **telnet port** Port to listen for telnet connections on
   * **max nodes** Maximum number of nodes (similtanious connections)
   * **default tagline** Default tagline for networked message bases, can be overridden.
   * **sysop name** The username belonging to the Sysop
   * **bbs name** The name of your bbs
 * Path Section
   * **gfile path** Path to where gfiles are located (Relative to working directory)
   * **data path** Path to where data files are (Relative to working directory)
   * **message path** Path to where message base data is stored (Relative to working directory)
   * **log path** Path where to store log files (Relative to working directory)
   * **temp path** Base path for temporary files eg. Drop files (Relative to working directory)
   * **scripts path** Path to where the JavaScript scripts live (Relative to working directory)
  
### msgbases.toml

Contains an array of `msgbase` lives in the `data path`

Example:
```
[[msgbase]]
name = "FSXNET: General Chat"
file = "fsx_gen"
type = "echomail"
aka = "21:1/999"
write-sec = 25
```

  * **name** The human readable name of the message base
  * **file** The filename (without extension) of the JAM message base containing messages (relative to the `message path` in envy.ini)
  * **type** either local, echomail or netmail
  * **aka** the aka for the network this message base is a part of (only required for netmail / echomail)
  * **write-sec** the minimum security level a user must have to write in the area
  * **read-sec** the minimum security level a user must be for the area to be visible at all

### doors.toml

Contains a array of `door` lives in the `data path`

Example:
```
[[door]]
key = "WORDELLE"
name = "Wordelle"
script = "/home/andrew/doors/wordelle/wordelle.sh"
```

  * **key** a key to use to execute this door via javascript.
  * **name** The human readable name of the door
  * **script** script to run to launch the door, is passed the nodenumber as argument 1

### seclevels.toml

Contains an array of `seclevel` lives in the `data path`

Example:
```
[[seclevel]]
level = 10
name = "New User"
timeout = 15
time = 120
```

  * **level** the security level
  * **name** a name for the security level
  * **timeout** the number of minutes a user of this security can idle before being booted
  * **time** the number of minutes a user of this security level has per day

### trashcan.txt

Each line contains a username that is unavailable to new users

Example:
```
sysop
unknown
guest
nobody
```

## Javascript Reference

  * **print(a_string)** Prints a string to the screen.
  * **getch()** Returns a pressed character, does not echo to the screen.
  * **disconnect()** Hangs up.
  * **gets(an_int)** Gets a string from the user, pass maximum length as integer
  * **getpass(an_int)** Same as gets but echos a mask character '*'.
  * **getattr(a_string1, a_string2)** Gets an attribute for the current user, a_string1 is the attribute name, a_string2 is the fallback value.
  * **putattr(a_string1, a_string2)** Puts an attribute for the current user, a_string1 is the attribute name, a_string2 is the value.
  * **getattro(an_int, a_string1, a_string2)** Gets an attribute for other user with an_int as uid. a_string1 is the attribute, a_string2 is the fallback value.
  * **putattro(an_int, a_string1, a_string2)** Puts an attribute for other user with an_int as uid. a_string1 is the attribute, a_string2 is the value.
  * **cls()** clears the screen
  * **gfile(a_string)** displays a gfile whith the filename a_string (must be in gfiles directory and no extension passed). Returns true if a gfile was sent, false if not.
  * **exec(a_string)** executes another script with the filename a_string (must be in the script directory and no extension passed).
  * **save(a_string1, a_string2)** Saves an attribute in the script storage (accessable only by this script). a_string1 is attribute, a_string2 value.
  * **load(a_string1, a_string2)** Loads an attribute from the script storage (accessable only by this script). a_string is attribute, a_string2 fallback value. Returns the attribute value.
  * **globalsave(a_string1, a_string2)** Saves an attribute in the global script storage (accessable by any script). a_string1 is attribute, a_string2 value.
  * **globalload(a_string1, a_string2)** Loads an attribute from the global script storage (accessable by any script). a_string1 is attribute, a_string2 fallback value. Returns the attribute value.
  * **getusername()** Returns the logged in user's username.
  * **listmsgs(an_int)** Displays a list of messages in the current message base, pass an_int for the msgno to start from.
  * **readmsgs(an_int)**  Displays messages in the current message base, pass an_int for the msgno to start from.
  * **writemsg()** Write a message in the current message base (first prompting the user for To, Subject fields).
  * **selectarea()** Allow the user to select the current message base.
  * **getarea()** Get the name of the current message base.
  * **pause()** Display a pause prompt.
  * **scanbases()** List message bases with unread and message totals.
  * **rundoor(a_string)** Runs a door, a_string is the key set in doors.toml
  * **getdoors()** Get an array of objects describing all dooes installed on the system.
```
[
  {
    "key": "SOMEKEY",
    "name": "Some Door"
  },
  {
    "key": "SOMEOTHERKEY",
    "name": "Another door"
  }
]
```

  * **getmsglr()** Returns the last read message in the current message base.
  * **getmsgtot()** Returns the total messages in the current message base.
  * **listemail()** Lists a user's email.
  * **enteremail(a_string1, a_string2)** Enter an email, to a_string1 with subject a_string2. **NOTE:** this does not check if a_string1 exists as a valid user. Be sure to check that first.
  * **checkuser(a_string1)** Checks if a_string1 is a user, returns a string with the user's username or empty string on failure.
  * **countemail()** How many emails a user has in his inbox.
  * **unreademail()** How many unread emails a user has in his inbox.
  * **timeleft()** The number of minutes a user has left for the day.
  * **getusers()** Returns an array of objects with the uid and username of all users.
```
  [
    {
      "uid": 1,
      "username": "Some User"
    },
    {
      "uid": 2,
      "username": "Some Other User"
    }
  ]
```

  * **opname()** Returns the sysop's user name as defined in `envy.ini`
  * **getversion()** Returns an object with various info on the version running.
```
{
  "envy": "0.1-a6787f",
  "system": "Linux",
  "sysver": "6.11.2",
  "machine": "x86_64",
  "duktape": "2.4.0"
}
```
  * **setaction(a_string)** Sets the current action the user is doing on the BBS, used for node info.
  * **getactions()** Gets an array of actions for each node.
```
[
  {
    "uid": 1,
    "action": "Browsing main menu"
  },
  {
    "uid": 0,
    "action": "Waiting for Caller"
  }

]
```

  * **getusernameo(an_int)** returns the username of a user with the uid matching an_int.
  * **setpasswordo(an_int, a_string)** Sets the password for a user with the uid of an_int to a_string.
  * **setpassword(a_string)** Sets the current user's password to a_string.
  * **checkpassword(a_string)** Returns true if a_string matches the current user's password.
  * **isvisible()** Returns true if the current user's security level is not invisible.
  * **readmsg(a_string, an_int)** Returns an object representing a message, a_string is the filename (minus extension) of the message base and an_int is the message id. If the message id does not exist (eg. it was deleted) it will return the next message (if any in the messagebase), if there are no appropriate messages, returns undefined. idx is the message number of the actual message. Line breaks are denoted as a carraige return only.
```
{
  "idx": 1,
  "from": "The Sysop",
  "to": "A User",
  "subject": "Stuff",
  "body": "Blah Blah\rMore Blah"
}
```

  * **postmsg(a_string1, a_string2, a_string3, a_string4, a_string5)** Posts a message into the message base with the filename (minus the extension) of a_string1, a_string2 is the "To" Field, a_string3 is the "From" field, a_string4 is the "Subject" field, and a_string5 is the "Body" (Note: new lines are denoted with a carraige return only).
