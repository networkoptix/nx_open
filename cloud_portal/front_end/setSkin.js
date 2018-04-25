const fsmv = require('fs-extra');
const path = require('path');
const copySkin = (color) => {
	var source = path.resolve(__dirname, '../skins', color, 'front_end/styles');
	var dest = path.resolve(__dirname, 'app/styles/custom');
	fsmv.copy(source, dest, {mkdirp:true}, error => error ? console.log(error) : null);

	source = path.resolve(__dirname, '../skins', color, 'front_end/images');
	dest = path.resolve(__dirname, 'app/images');
	fsmv.copy(source, dest, {mkdirp:true}, error => error ? console.log(error) : null);
}

copySkin(process.argv[3] || 'blue');