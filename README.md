# README

This repository holds my now defunct code implementation of a multiplayer battle arena game written for my EE120B class at UC, Riverside. At some point, I hope to port this, or modernize it to a new platform that is more readily available.

Completed June 3, 2013. Work conducted over the course of two weeks.

# Multiplayer Battle Arena

Video Link: https://www.youtube.com/watch?v=jeQN9_5kBX4 

## Description
A game where two players attempt to deplete the opposing player's life points. Weapons are used to aid in injuring the other player and random events keep the game pace interesting.

## Technologies
AVR Studio 6, ATmega32, HD44780U LCD, two 16-button keypads, button

## Guide
Each player controls their character with a keypad (controls defined below). First player to bring the other player's life points to zero wins. If a player collides with a hostile particle, like a bullet or explosion, they lose a life point and become immune to damage for a short period. 

Every set interval, the game will generate an event from a set of events, like generate a weapon on the center field, generate a rain effect, or create a lethal lava floor which can damage players. A map-like interface below the playing field indicates where/what is in an item box or where a player is. The map interface also issues warnings about any weather events like the lava field. Colliding the player avatar with item pickups will add some ammo of that weapon type to their inventory, allowing them to swap to that weapon and fire it. 

There are 8 weapons with varying effects with a  unique designating letter: pistol, rifle, railgun, wavecannon, rocket, machine-gun, clusterbomb, healing. On the side is a display which displays the player’s life, weapon cooldown, the equipped weapon, and its ammo left.

## Controls
Buttons are given in order of priority, pressing any higher order button will inhibit any other buttons pressed; both players follow the same control scheme on different keypads; on the board is a reset button

|   |                                                                                                                             |
|---|-----------------------------------------------------------------------------------------------------------------------------|
| 7 | Fire currently equipped weapon and depletes ammo count with that weapon, continues previous movement vector                 |
| 8 | Cycle backwards through weapons with positive ammo                                                                          |
| 9 | Jetpack pushes players upwards; if character was already moving horizontally, keep that vector and add a vertical component |
| C | Cycle forwards through weapons with positive ammo                                                                           |
| * | Same as 7, less priority                                                                                                    |
| 0 | Move left                                                                                                                   |
| D | Move right                                                                                                                  |


### Files
projectC.c- main source code with SM and game logic implementations

Following source files taken from the Lab 8 header file set here:
https://docs.google.com/folder/d/0B-mTsOswSyiKWWJ5clNtT1EtSVk/edit 

| | |
|-|-|
|bit.h| functions for simpler reading and writing on binary numbers|
|keypad.h| included header for interfacing with the keypad, with minor modifications for the second player's input|
|timer.h| included header file for use in the task structure|
