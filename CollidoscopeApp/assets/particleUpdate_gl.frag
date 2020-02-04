#version 150 core

uniform vec2 uCloudCentre;
uniform float uRadius;
uniform float uDamping;

in vec2 iPosition;
in vec2 iDirection;
in vec2 iCloudCentre;
in float iLifeSpan;
in float iDamping;

out vec2  position;
out vec2  direction;
out vec2 cloudCentre;
out float lifeSpan;
out float damping;


void main()
{
  direction = iDirection;
  cloudCentre = iCloudCentre; // to carry the value over 
  position = iPosition + direction;

  if (iDamping > uDamping ||                                  // -> filters out some particle to make the cloud less dense when the sound is low pass filtered 
      length(position - iCloudCentre) > uRadius - iLifeSpan)  // -> reset particle to cloud centre if it's gone beyond the cloud radius 
  {
    position = uCloudCentre;
    cloudCentre = uCloudCentre;

  }

  damping = iDamping;
  lifeSpan = iLifeSpan;
}

