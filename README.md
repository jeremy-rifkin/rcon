# RCON
A simple client for Valve's RCON protocol. Currently only supports Windows.

Usage:

```
rcon <host> [options]
<host> can be an ip or domain name and can include the port.
Options:
    -p <port>     RCON Port. Overrides port specified along with host, if present.
    -P <password> RCON Password. If not present the user will be prompted for a password.
    -c <command>  Command to execute. If present, the program will connect, authenticate, run the command, and then exit.
    -s            Silent mode. Only relevant if -c is present. Silent mode will suppress everything except command response.
    -S            Use a secure protocol. TBA.
    -h --help     Display help message
    -M            Support Minecraft colors and other formatting codes.
```

Example usage:
```
rcon 10.0.0.2 -pPMc 25575 password command
```
Connects to `10.0.0.2` on port `25575` using password `password`, handles Minecraft formatting codes, and runs the command `command`, displays the results, and then exits.

```
rcon 10.0.0.2:25575 -c command -s -M
```
Connects to `10.0.0.2` on port `25575`, runs a command, accepts Minecraft formatting codes, and silences all output except for the command response. Since no password was specified, the user will be prompted for a password.

# License

Copyright 2019 Felis Phasma.
