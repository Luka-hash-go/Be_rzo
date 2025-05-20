### Demontrer le besoin d'un protocole plus evolué que TCP pour transporter la video dans de bonnes conditions 
Tcp a son mécanisme de reprise des pertes qui ne permet la délivrance d’une donnée applicative que si toutes les données précédentes ont été délivrées.
Ce qui s'avere problematique dans le cadre de notre be reseau ou l'on doit transferer de la video en temps réel.
Si on passe par TCP on aura une dégradation de la présentation lorsque le réseau présente un fort % de perte.
