function OnAfterSceneLoaded()
  -- debug initialization
  Debug:Enable(true)
  Debug:PrintLine("Debug enabled!")
  -- camera setup
	local myCamera = Game:GetEntity("CameraPosition")
  local mainCamera = Game:GetCamera()
  mainCamera:SetUseEulerAngles(true)
  mainCamera:SetPosition(myCamera:GetPosition())
  mainCamera:SetOrientation(myCamera:GetOrientation())
end
