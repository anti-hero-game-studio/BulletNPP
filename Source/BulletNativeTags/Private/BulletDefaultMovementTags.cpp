// Fill out your copyright notice in the Description page of Project Settings.


#include "BulletDefaultMovementTags.h"


#define LOCTEXT_NAMESPACE "BulletData"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bullet_IsOnGround, "Bullet.IsOnGround", "Default Bullet state flag indicating character is on the ground.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bullet_IsInAir, "Bullet.IsInAir", "Default Bullet state flag indicating character is in the air.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bullet_IsFalling, "Bullet.IsFalling", "Default Bullet state flag indicating character is falling.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bullet_IsFlying, "Bullet.IsFlying", "Default Bullet state flag indicating character is flying.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bullet_IsSwimming, "Bullet.IsSwimming", "Default Bullet state flag indicating character is swimming.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bullet_IsCrouching, "Bullet.Stance.IsCrouching", "Default Bullet state flag indicating character is crouching.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bullet_IsNavWalking, "Bullet.IsNavWalking", "Default Bullet state flag indicating character is NavWalking.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bullet_SkipAnimRootMotion, "Bullet.SkipAnimRootMotion", "Default Bullet state flag indicating Animation Root Motion proposed movement should be skipped.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(Bullet_SkipVerticalAnimRootMotion, "Bullet.SkipVerticalAnimRootMotion", "Default Bullet state flag indicating Animation Root Motion proposed movements should not include a vertical velocity component (along the up/down axis).");
