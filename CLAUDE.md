# MMO Client Project Guide

## Project Overview
This is an Unreal Engine 5.7 MMO client with a Go WebSocket server. The game features third-person melee combat with combo attacks, charged attacks, and server-authoritative multiplayer.

## Architecture

### Client (Unreal Engine 5.7)
- **Location**: `/mnt/c/Users/Jeff/Documents/Unreal Projects/mmoclient`
- **Language**: C++ with Blueprint extensions
- **Input System**: Enhanced Input System (not legacy)

### Server (Go)
- **Location**: `/home/enum/Projects/mmo-server`
- **Entry Point**: `main.go`
- **Protocol**: WebSocket on `ws://localhost:8080/ws`
- **Run**: `cd /home/enum/Projects/mmo-server && go run main.go`

## Key Source Files

### Combat System (`Source/mmoclient/Variant_Combat/`)
- `CombatCharacter.h/.cpp` - Base character with melee combat, HP, ragdoll death
- `CombatAttacker.h` - Interface for attack traces
- `CombatDamageable.h` - Interface for receiving damage

### Network (`Source/mmoclient/Variant_Combat/Network/`)
- `CombatNetworkSubsystem.h/.cpp` - WebSocket connection, state sync, message handlers
- `CombatRemotePlayer.h/.cpp` - Represents other players, interpolation, hit reactions
- `CombatNetworkTypes.h` - Network state structs, animation enums

### Blueprints (`Content/Variant_Combat/Blueprints/`)
- `BP_CombatCharacter` - Local player blueprint (has ReceivedDamage event implementation)
- `BP_CombatRemotePlayer` - Remote player blueprint (inherits from C++ ACombatRemotePlayer)
- `BP_CombatGameMode` - Sets up network connection on BeginPlay

## Server-Authoritative Features
- **HP/Damage**: Server controls all HP, clients cannot modify their own
- **Attack Validation**: Server checks range (200 units), cooldown (0.5s)
- **Position Validation**: Max speed 600 units/s, server corrects cheaters
- **Respawn**: Server controls respawn timing (5s) and position

## Build Notes

### Live Coding
- `Ctrl+Alt+F11` to compile while editor is running
- Does NOT work well with header changes or new/removed functions
- For significant changes, close editor and rebuild

### Build Configuration
UBA and adaptive unity build are disabled in `C:\Users\Jeff\AppData\Roaming\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml`:
```xml
<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
    <BuildConfiguration>
        <bAllowUBAExecutor>false</bAllowUBAExecutor>
        <bUseAdaptiveUnityBuild>false</bUseAdaptiveUnityBuild>
    </BuildConfiguration>
</Configuration>
```

### Unity Build
Source compiles into `Module.mmoclient.1.cpp`, `.2.cpp`, `.3.cpp` - this is normal Unity Build behavior, not separate modules.

## Common Issues

### Ragdoll Falls Through Floor
- Mesh collision must respond to both `ECC_WorldStatic` AND `ECC_WorldDynamic`
- Floor in this project uses `BlockAllDynamic` preset (WorldDynamic object type)
- Physics Asset bodies need proper collision setup

### Remote Player Hit Reaction
- Uses physics blend (SetPhysicsBlendWeight) NOT full ragdoll
- Pelvis bone must be locked (SetBodySimulatePhysics(pelvis, false))
- Timer resets blend after 0.5s since Landed() may not trigger

### Ghost Players on Server
- Happens when PIE stops abruptly without proper disconnect
- Fix: Restart the Go server to clear

### BP Properties Not Inherited
- BP_CombatRemotePlayer inherits from ACombatRemotePlayer (C++), NOT BP_CombatCharacter
- Blueprint event implementations (like ReceivedDamage) don't transfer through C++ inheritance
- Must copy BP event handlers or implement in C++

## Network Message Types

### Client -> Server
- `join` - Initial connection with player name
- `state_update` - Position, rotation, velocity, animation state
- `attack` - Attack request with target_id
- `leave` - Graceful disconnect

### Server -> Client
- `join_response` - Player ID and spawn position (X/Y only, client finds Z)
- `player_joined` - Another player connected
- `player_state` - Remote player state update
- `player_left` - Player disconnected
- `position_correction` - Anti-cheat position reset
- `damage` - Attack hit, includes target HP and dead status
- `respawn` - Player respawned with new position and HP

## Testing Multiplayer
1. Start server: `cd /home/enum/Projects/mmo-server && go run main.go`
2. Open two PIE instances in Unreal Editor (or packaged builds)
3. Each client auto-connects on BeginPlay via BP_CombatGameMode
